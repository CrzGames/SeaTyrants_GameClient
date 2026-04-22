#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import json
import math
import argparse
import itertools
from typing import Dict, List, Tuple, Optional

try:
    import cv2
    import numpy as np
except ImportError as exc:
    missing = str(exc).split()[-1].strip("'")
    print("Erreur: dépendance manquante.")
    print("Installe Python puis exécute :")
    print("  pip install opencv-python numpy")
    print(f"Module manquant détecté: {missing}")
    sys.exit(1)


# ============================================================
# CONSTANTES
# ============================================================

DIRECTIONS = ["bas_gauche", "haut_droite", "haut_gauche", "bas_droite"]
STATES = ["full", "low"]

PAIR_BOTTOM = ("bas_gauche", "bas_droite")
PAIR_TOP = ("haut_gauche", "haut_droite")

CANONICAL_EXPORT = [
    ("1.png", "full", "bas_gauche"),
    ("2.png", "full", "haut_droite"),
    ("3.png", "full", "haut_gauche"),
    ("4.png", "full", "bas_droite"),
    ("5.png", "low",  "bas_gauche"),
    ("6.png", "low",  "haut_droite"),
    ("7.png", "low",  "haut_gauche"),
    ("8.png", "low",  "bas_droite"),
]


# ============================================================
# UTILITAIRES
# ============================================================

def ensure_dir(path: str) -> None:
    os.makedirs(path, exist_ok=True)


def clamp01(x: float) -> float:
    return max(0.0, min(1.0, float(x)))


def jsonable(obj):
    if isinstance(obj, dict):
        return {str(k): jsonable(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [jsonable(v) for v in obj]
    if isinstance(obj, np.integer):
        return int(obj)
    if isinstance(obj, np.floating):
        return float(obj)
    if isinstance(obj, np.ndarray):
        return obj.tolist()
    return obj


# ============================================================
# I/O
# ============================================================

def load_rgba(path: str) -> np.ndarray:
    img = cv2.imread(path, cv2.IMREAD_UNCHANGED)
    if img is None:
        raise FileNotFoundError(f"Impossible d'ouvrir l'image: {path}")
    if img.ndim != 3 or img.shape[2] != 4:
        raise ValueError(f"L'image doit être en RGBA: {path}")
    return img


def save_rgba(path: str, img: np.ndarray) -> None:
    ok = cv2.imwrite(path, img)
    if not ok:
        raise RuntimeError(f"Échec lors de l'écriture: {path}")


def load_images(folder: str) -> Dict[int, np.ndarray]:
    imgs: Dict[int, np.ndarray] = {}
    for i in range(1, 9):
        path = os.path.join(folder, f"{i}.png")
        if not os.path.isfile(path):
            raise FileNotFoundError(f"Fichier manquant: {path}")
        imgs[i] = load_rgba(path)
    return imgs


def load_mapping(mapping_path: str) -> Dict[int, Dict[str, str]]:
    with open(mapping_path, "r", encoding="utf-8") as f:
        raw = json.load(f)

    mapping: Dict[int, Dict[str, str]] = {}

    if not isinstance(raw, dict):
        raise ValueError("mapping.json doit contenir un objet JSON.")

    for key, value in raw.items():
        try:
            img_id = int(key)
        except ValueError:
            raise ValueError(f"Clé invalide dans mapping.json: {key}")

        if img_id < 1 or img_id > 8:
            raise ValueError(f"Clé hors plage 1..8 dans mapping.json: {img_id}")

        if not isinstance(value, dict):
            raise ValueError(f"Valeur invalide pour {img_id} dans mapping.json.")

        state = value.get("state")
        direction = value.get("direction")

        if state not in STATES:
            raise ValueError(f"state invalide pour {img_id}: {state}")
        if direction not in DIRECTIONS:
            raise ValueError(f"direction invalide pour {img_id}: {direction}")

        mapping[img_id] = {
            "state": state,
            "direction": direction
        }

    return mapping


def validate_mapping(mapping: Dict[int, Dict[str, str]]) -> None:
    if set(mapping.keys()) != set(range(1, 9)):
        missing = sorted(set(range(1, 9)) - set(mapping.keys()))
        extra = sorted(set(mapping.keys()) - set(range(1, 9)))
        raise ValueError(f"mapping.json incomplet. missing={missing}, extra={extra}")

    seen = set()
    for img_id, item in mapping.items():
        key = (item["state"], item["direction"])
        if key in seen:
            raise ValueError(f"Doublon dans mapping.json: {key}")
        seen.add(key)

    expected = {(s, d) for s in STATES for d in DIRECTIONS}
    if seen != expected:
        missing = sorted(expected - seen)
        extra = sorted(seen - expected)
        raise ValueError(f"mapping.json invalide. missing={missing}, extra={extra}")


def build_group_ids(mapping: Dict[int, Dict[str, str]]) -> Dict[str, Dict[str, int]]:
    out = {
        "full": {},
        "low": {},
    }
    for img_id, item in mapping.items():
        out[item["state"]][item["direction"]] = img_id
    return out


# ============================================================
# OUTILS IMAGE
# ============================================================

def alpha_mask(img: np.ndarray, thr: int = 16) -> np.ndarray:
    return img[:, :, 3] > thr


def crop_to_alpha(img: np.ndarray, pad: int = 2) -> np.ndarray:
    mask = alpha_mask(img)
    ys, xs = np.where(mask)
    if xs.size == 0:
        return img.copy()

    x0 = max(0, int(xs.min()) - pad)
    y0 = max(0, int(ys.min()) - pad)
    x1 = min(img.shape[1], int(xs.max()) + 1 + pad)
    y1 = min(img.shape[0], int(ys.max()) + 1 + pad)
    return img[y0:y1, x0:x1].copy()


def fit_on_canvas(img: np.ndarray, size: int = 96) -> np.ndarray:
    crop = crop_to_alpha(img, pad=2)
    h, w = crop.shape[:2]
    if h <= 0 or w <= 0:
        return np.zeros((size, size, 4), dtype=np.uint8)

    scale = min(size / max(1, w), size / max(1, h))
    nw = max(1, int(round(w * scale)))
    nh = max(1, int(round(h * scale)))

    interp = cv2.INTER_AREA if scale < 1.0 else cv2.INTER_LINEAR
    resized = cv2.resize(crop, (nw, nh), interpolation=interp)

    canvas = np.zeros((size, size, 4), dtype=np.uint8)
    x = (size - nw) // 2
    y = (size - nh) // 2
    canvas[y:y + nh, x:x + nw] = resized
    return canvas


def shift_rgba(img: np.ndarray, dx: int, dy: int) -> np.ndarray:
    h, w = img.shape[:2]
    m = np.float32([[1, 0, dx], [0, 1, dy]])
    return cv2.warpAffine(
        img,
        m,
        (w, h),
        flags=cv2.INTER_NEAREST,
        borderMode=cv2.BORDER_CONSTANT,
        borderValue=(0, 0, 0, 0)
    )


def premultiplied_bgr(img: np.ndarray) -> np.ndarray:
    bgr = img[:, :, :3].astype(np.float32)
    alpha = (img[:, :, 3].astype(np.float32) / 255.0)[..., None]
    out = np.clip(bgr * alpha, 0, 255).astype(np.uint8)
    return out


def masked_gray(img: np.ndarray) -> np.ndarray:
    gray = cv2.cvtColor(premultiplied_bgr(img), cv2.COLOR_BGR2GRAY)
    gray = cv2.GaussianBlur(gray, (3, 3), 0.8)
    return gray


def connected_components_stats(mask_bool: np.ndarray):
    mask_u8 = (mask_bool.astype(np.uint8) * 255)
    return cv2.connectedComponentsWithStats(mask_u8, connectivity=8)


def largest_component_area(mask_bool: np.ndarray) -> int:
    n, _labels, stats, _ = connected_components_stats(mask_bool)
    if n <= 1:
        return 0
    return int(np.max(stats[1:, cv2.CC_STAT_AREA]))


def sharpness_score(img: np.ndarray) -> float:
    mask = alpha_mask(img)
    if np.count_nonzero(mask) < 20:
        return 0.0
    gray = masked_gray(img)
    lap = cv2.Laplacian(gray, cv2.CV_32F)
    vals = lap[mask]
    if vals.size == 0:
        return 0.0
    return float(vals.var())


# ============================================================
# CARTES DE DÉTAILS INTERNES
# ============================================================

def inner_mask(img: np.ndarray, thr: int = 16, erode_px: int = 3) -> np.ndarray:
    mask = alpha_mask(img, thr=thr)
    if np.count_nonzero(mask) == 0:
        return mask

    k = 2 * erode_px + 1
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (k, k))
    eroded = cv2.erode((mask.astype(np.uint8) * 255), kernel, iterations=1) > 0

    if np.count_nonzero(eroded) < 20:
        return mask

    return eroded


def internal_edge_map(img: np.ndarray, size: int = 96) -> np.ndarray:
    canvas = fit_on_canvas(img, size=size)
    gray = masked_gray(canvas)
    edges = cv2.Canny(gray, 40, 120) > 0
    inside = inner_mask(canvas, thr=16, erode_px=3)
    return edges & inside


def internal_texture_map(img: np.ndarray, size: int = 96) -> np.ndarray:
    canvas = fit_on_canvas(img, size=size)
    gray = masked_gray(canvas)
    inside = inner_mask(canvas, thr=16, erode_px=3)

    gx = cv2.Sobel(gray, cv2.CV_32F, 1, 0, ksize=3)
    gy = cv2.Sobel(gray, cv2.CV_32F, 0, 1, ksize=3)
    mag = np.sqrt(gx * gx + gy * gy)

    mmax = float(np.max(mag))
    if mmax > 1e-6:
        mag = mag / mmax

    mag = mag * inside.astype(np.float32)
    return mag


# ============================================================
# DESCRIPTEURS
# ============================================================

def make_descriptor(img: np.ndarray) -> np.ndarray:
    canvas = fit_on_canvas(img, 96)

    mask = alpha_mask(canvas).astype(np.float32)
    edges = internal_edge_map(canvas, size=96).astype(np.float32)
    bgr = premultiplied_bgr(canvas).astype(np.float32) / 255.0

    alpha_small = cv2.resize(mask, (24, 24), interpolation=cv2.INTER_AREA).reshape(-1)
    edge_small = cv2.resize(edges, (24, 24), interpolation=cv2.INTER_AREA).reshape(-1)
    rgb_small = cv2.resize(bgr, (12, 12), interpolation=cv2.INTER_AREA).reshape(-1)

    m = alpha_mask(canvas)
    h, w = m.shape
    top_profile = np.full((w,), h, dtype=np.float32)
    bot_profile = np.zeros((w,), dtype=np.float32)

    for x in range(w):
        ys = np.where(m[:, x])[0]
        if ys.size > 0:
            top_profile[x] = float(ys.min())
            bot_profile[x] = float(ys.max())

    top_profile /= max(1.0, float(h - 1))
    bot_profile /= max(1.0, float(h - 1))

    top_small = cv2.resize(top_profile[None, :], (32, 1), interpolation=cv2.INTER_AREA).reshape(-1)
    bot_small = cv2.resize(bot_profile[None, :], (32, 1), interpolation=cv2.INTER_AREA).reshape(-1)

    vec = np.concatenate([
        alpha_small * 1.20,
        edge_small * 1.00,
        rgb_small * 0.80,
        top_small * 0.80,
        bot_small * 0.80,
    ]).astype(np.float32)

    return vec


# ============================================================
# COMPARAISON MIROIR
# ============================================================

def compare_aligned(a: np.ndarray, b: np.ndarray) -> Dict[str, float]:
    mask_a = alpha_mask(a)
    mask_b = alpha_mask(b)

    inter = mask_a & mask_b
    union = mask_a | mask_b

    union_area = int(np.count_nonzero(union))
    inter_area = int(np.count_nonzero(inter))

    if union_area == 0:
        return {
            "score": 0.0,
            "iou": 0.0,
            "rgb_similarity": 0.0,
            "edge_similarity": 0.0,
            "exclusive_a_ratio": 1.0,
            "exclusive_b_ratio": 1.0,
            "largest_exclusive_a_ratio": 1.0,
            "largest_exclusive_b_ratio": 1.0,
        }

    iou = inter_area / union_area

    inter_u8 = (inter.astype(np.uint8) * 255)
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    inter_eroded = cv2.erode(inter_u8, kernel, iterations=1) > 0
    common = inter_eroded if np.count_nonzero(inter_eroded) >= 20 else inter

    if np.count_nonzero(common) > 0:
        rgb_a = a[:, :, :3].astype(np.float32) / 255.0
        rgb_b = b[:, :, :3].astype(np.float32) / 255.0
        rgb_diff = np.mean(np.abs(rgb_a - rgb_b), axis=2)
        rgb_similarity = 1.0 - float(np.mean(rgb_diff[common]))
        rgb_similarity = clamp01(rgb_similarity)
    else:
        rgb_similarity = 0.0

    gray_a = masked_gray(a)
    gray_b = masked_gray(b)

    edge_a = cv2.Canny(gray_a, 40, 120) > 0
    edge_b = cv2.Canny(gray_b, 40, 120) > 0

    edge_union = edge_a | edge_b
    edge_union_area = int(np.count_nonzero(edge_union))
    if edge_union_area > 0:
        edge_xor = edge_a ^ edge_b
        edge_similarity = 1.0 - (np.count_nonzero(edge_xor) / edge_union_area)
        edge_similarity = clamp01(edge_similarity)
    else:
        edge_similarity = 1.0

    exclusive_a = mask_a & (~mask_b)
    exclusive_b = mask_b & (~mask_a)

    exclusive_a_ratio = float(np.count_nonzero(exclusive_a) / union_area)
    exclusive_b_ratio = float(np.count_nonzero(exclusive_b) / union_area)

    largest_exclusive_a_ratio = float(largest_component_area(exclusive_a) / union_area)
    largest_exclusive_b_ratio = float(largest_component_area(exclusive_b) / union_area)

    score = 100.0 * (
        0.55 * iou +
        0.30 * rgb_similarity +
        0.15 * edge_similarity
    )

    return {
        "score": float(score),
        "iou": float(iou),
        "rgb_similarity": float(rgb_similarity),
        "edge_similarity": float(edge_similarity),
        "exclusive_a_ratio": float(exclusive_a_ratio),
        "exclusive_b_ratio": float(exclusive_b_ratio),
        "largest_exclusive_a_ratio": float(largest_exclusive_a_ratio),
        "largest_exclusive_b_ratio": float(largest_exclusive_b_ratio),
    }


def compare_pair_mirror(a: np.ndarray, b: np.ndarray, max_shift: int = 5) -> Dict[str, float]:
    a = fit_on_canvas(a, 96)
    b = fit_on_canvas(b, 96)
    b_flip = cv2.flip(b, 1)

    best: Optional[Dict[str, float]] = None

    for dy in range(-max_shift, max_shift + 1):
        for dx in range(-max_shift, max_shift + 1):
            shifted = shift_rgba(b_flip, dx, dy)
            metrics = compare_aligned(a, shifted)
            metrics["dx"] = float(dx)
            metrics["dy"] = float(dy)

            if best is None or metrics["score"] > best["score"]:
                best = metrics

    assert best is not None
    return best


def pair_base_penalty(metrics: Dict[str, float], keep_left: bool) -> float:
    if keep_left:
        exc = float(metrics["exclusive_a_ratio"])
        big = float(metrics["largest_exclusive_a_ratio"])
    else:
        exc = float(metrics["exclusive_b_ratio"])
        big = float(metrics["largest_exclusive_b_ratio"])

    # Plus ce score est élevé, plus la base gardée paraît contenir des détails exclusifs/suspects
    penalty = 100.0 * (0.70 * exc + 0.30 * big)
    return float(penalty)


# ============================================================
# MÉTA PAR IMAGE
# ============================================================

def build_meta(imgs: Dict[int, np.ndarray]) -> Dict[int, dict]:
    meta: Dict[int, dict] = {}

    for img_id, img in imgs.items():
        img_flip = cv2.flip(img, 1)

        edge_orig = cv2.resize(
            internal_edge_map(img, size=96).astype(np.float32),
            (32, 32),
            interpolation=cv2.INTER_AREA
        ) > 0.20

        edge_flip = cv2.resize(
            internal_edge_map(img_flip, size=96).astype(np.float32),
            (32, 32),
            interpolation=cv2.INTER_AREA
        ) > 0.20

        tex_orig = cv2.resize(
            internal_texture_map(img, size=96).astype(np.float32),
            (32, 32),
            interpolation=cv2.INTER_AREA
        ) > 0.18

        tex_flip = cv2.resize(
            internal_texture_map(img_flip, size=96).astype(np.float32),
            (32, 32),
            interpolation=cv2.INTER_AREA
        ) > 0.18

        meta[img_id] = {
            "variants": {
                0: img,
                1: img_flip,
            },
            "descriptor": {
                0: make_descriptor(img),
                1: make_descriptor(img_flip),
            },
            "edge_small": {
                0: edge_orig,
                1: edge_flip,
            },
            "tex_small": {
                0: tex_orig,
                1: tex_flip,
            },
            "sharpness": {
                0: sharpness_score(img),
                1: sharpness_score(img_flip),
            }
        }

    return meta


def build_pair_metrics(groups: Dict[str, Dict[str, int]], imgs: Dict[int, np.ndarray]) -> Dict[str, Dict[str, dict]]:
    out: Dict[str, Dict[str, dict]] = {"full": {}, "low": {}}

    for state in STATES:
        g = groups[state]

        bg = g["bas_gauche"]
        bd = g["bas_droite"]
        hg = g["haut_gauche"]
        hd = g["haut_droite"]

        out[state]["bottom"] = compare_pair_mirror(imgs[bg], imgs[bd], max_shift=5)
        out[state]["top"] = compare_pair_mirror(imgs[hg], imgs[hd], max_shift=5)

    return out


# ============================================================
# COHÉRENCE GLOBALE
# ============================================================

def rare_internal_details_penalty_from_refs(
    refs: List[Tuple[int, int]],
    meta: Dict[int, dict]
) -> Dict[str, float]:
    edge_maps = []
    tex_maps = []

    for ref in refs:
        img_id, state = ref
        edge_maps.append(meta[img_id]["edge_small"][state])
        tex_maps.append(meta[img_id]["tex_small"][state])

    edge_stack = np.stack(edge_maps, axis=0).astype(np.float32)
    tex_stack = np.stack(tex_maps, axis=0).astype(np.float32)

    edge_support = np.mean(edge_stack, axis=0)
    tex_support = np.mean(tex_stack, axis=0)

    rare_edge = (edge_support > 0.0) & (edge_support < 0.75)
    rare_tex = (tex_support > 0.0) & (tex_support < 0.75)

    very_rare_edge = (edge_support > 0.0) & (edge_support <= 0.26)
    very_rare_tex = (tex_support > 0.0) & (tex_support <= 0.26)

    denom_edge = max(1, int(np.count_nonzero(edge_support > 0.0)))
    denom_tex = max(1, int(np.count_nonzero(tex_support > 0.0)))

    rare_edge_ratio = float(np.count_nonzero(rare_edge) / denom_edge)
    rare_tex_ratio = float(np.count_nonzero(rare_tex) / denom_tex)

    very_rare_edge_ratio = float(np.count_nonzero(very_rare_edge) / denom_edge)
    very_rare_tex_ratio = float(np.count_nonzero(very_rare_tex) / denom_tex)

    rare_union = rare_edge | rare_tex
    n, _labels, stats, _ = connected_components_stats(rare_union)
    largest_comp_ratio = 0.0
    if n > 1:
        largest_area = int(np.max(stats[1:, cv2.CC_STAT_AREA]))
        largest_comp_ratio = float(largest_area / (32 * 32))

    penalty = (
        18.0 * rare_edge_ratio +
        12.0 * rare_tex_ratio +
        16.0 * very_rare_edge_ratio +
        10.0 * very_rare_tex_ratio +
        18.0 * largest_comp_ratio
    )

    return {
        "penalty": float(penalty),
        "rare_edge_ratio": float(rare_edge_ratio),
        "rare_tex_ratio": float(rare_tex_ratio),
        "very_rare_edge_ratio": float(very_rare_edge_ratio),
        "very_rare_tex_ratio": float(very_rare_tex_ratio),
        "largest_comp_ratio": float(largest_comp_ratio),
    }


def best_global_orientation_from_refs(
    refs: List[Tuple[int, int]],
    meta: Dict[int, dict]
) -> Dict[str, object]:
    best: Optional[Dict[str, object]] = None

    for bits in itertools.product([0, 1], repeat=len(refs)):
        oriented_refs = []
        descs = []

        for i, ref in enumerate(refs):
            img_id, state = ref
            actual_state = state ^ bits[i]
            oriented_refs.append((img_id, actual_state))
            descs.append(meta[img_id]["descriptor"][actual_state])

        stack = np.stack(descs, axis=0)
        median = np.median(stack, axis=0)

        dists = [float(np.mean(np.abs(d - median))) for d in descs]
        mean_dist = float(np.mean(dists))

        base_score = 100.0 * max(0.0, 1.0 - mean_dist)

        rare_info = rare_internal_details_penalty_from_refs(oriented_refs, meta)
        final_score = max(0.0, base_score - rare_info["penalty"])

        candidate = {
            "bits": list(bits),
            "oriented_refs": oriented_refs,
            "base_score": float(base_score),
            "mean_dist": float(mean_dist),
            "rare_penalty": float(rare_info["penalty"]),
            "rare_info": rare_info,
            "final_score": float(final_score),
        }

        if best is None or candidate["final_score"] > best["final_score"]:
            best = candidate

    assert best is not None
    return best


# ============================================================
# SOLVE PAR ÉTAT
# ============================================================

def build_refs_for_state(
    group_ids: Dict[str, int],
    keep_bottom_left: bool,
    keep_top_left: bool
) -> Dict[str, Tuple[int, int]]:
    """
    Retourne refs_by_dir:
      ref = (img_id, flip_state)
      flip_state 0 = original
      flip_state 1 = flip horizontal
    """
    refs: Dict[str, Tuple[int, int]] = {}

    bg = group_ids["bas_gauche"]
    bd = group_ids["bas_droite"]
    hg = group_ids["haut_gauche"]
    hd = group_ids["haut_droite"]

    # Paire bas
    if keep_bottom_left:
        refs["bas_gauche"] = (bg, 0)
        refs["bas_droite"] = (bg, 1)
    else:
        refs["bas_droite"] = (bd, 0)
        refs["bas_gauche"] = (bd, 1)

    # Paire haut
    if keep_top_left:
        refs["haut_gauche"] = (hg, 0)
        refs["haut_droite"] = (hg, 1)
    else:
        refs["haut_droite"] = (hd, 0)
        refs["haut_gauche"] = (hd, 1)

    return refs


def solve_state_group(
    state: str,
    group_ids: Dict[str, int],
    meta: Dict[int, dict],
    pair_metrics: Dict[str, dict]
) -> Dict[str, object]:
    bottom_metrics = pair_metrics[state]["bottom"]
    top_metrics = pair_metrics[state]["top"]

    pair_support_bottom = float(bottom_metrics["score"])
    pair_support_top = float(top_metrics["score"])
    pair_support_mean = float((pair_support_bottom + pair_support_top) / 2.0)

    all_solutions = []

    for keep_bottom_left in [True, False]:
        for keep_top_left in [True, False]:
            refs_by_dir = build_refs_for_state(group_ids, keep_bottom_left, keep_top_left)
            refs_ordered = [refs_by_dir[d] for d in DIRECTIONS]

            global_info = best_global_orientation_from_refs(refs_ordered, meta)

            bottom_pen = pair_base_penalty(bottom_metrics, keep_left=keep_bottom_left)
            top_pen = pair_base_penalty(top_metrics, keep_left=keep_top_left)
            base_penalty_mean = float((bottom_pen + top_pen) / 2.0)

            # Petite préférence pour la netteté des bases gardées
            bg = group_ids["bas_gauche"]
            bd = group_ids["bas_droite"]
            hg = group_ids["haut_gauche"]
            hd = group_ids["haut_droite"]

            kept_bottom_id = bg if keep_bottom_left else bd
            kept_top_id = hg if keep_top_left else hd

            sharp_bottom = float(meta[kept_bottom_id]["sharpness"][0])
            sharp_top = float(meta[kept_top_id]["sharpness"][0])
            sharp_mean = float((sharp_bottom + sharp_top) / 2.0)

            # normalisation légère
            sharp_bonus = min(8.0, math.log1p(max(0.0, sharp_mean)) * 1.5)

            total_score = (
                0.64 * float(global_info["final_score"]) +
                0.21 * pair_support_mean +
                0.10 * max(0.0, 100.0 - base_penalty_mean) +
                0.05 * sharp_bonus
            )

            solution = {
                "state": state,
                "keep_bottom_left": keep_bottom_left,
                "keep_top_left": keep_top_left,
                "refs_by_dir": refs_by_dir,
                "pair_support_bottom": pair_support_bottom,
                "pair_support_top": pair_support_top,
                "pair_support_mean": pair_support_mean,
                "base_penalty_bottom": float(bottom_pen),
                "base_penalty_top": float(top_pen),
                "base_penalty_mean": float(base_penalty_mean),
                "global_base_score": float(global_info["base_score"]),
                "global_rare_penalty": float(global_info["rare_penalty"]),
                "global_final_score": float(global_info["final_score"]),
                "rare_info": global_info["rare_info"],
                "consensus_oriented_refs": global_info["oriented_refs"],
                "sharp_bonus": float(sharp_bonus),
                "total_score": float(total_score),
            }
            all_solutions.append(solution)

    all_solutions.sort(key=lambda x: x["total_score"], reverse=True)
    best = all_solutions[0]
    second = all_solutions[1] if len(all_solutions) > 1 else None

    return {
        "state": state,
        "group_ids": group_ids,
        "best": best,
        "second": second,
        "confidence_gap": float(best["total_score"] - second["total_score"]) if second is not None else 999.0,
        "pair_metrics": {
            "bottom": bottom_metrics,
            "top": top_metrics,
        }
    }


# ============================================================
# EXPORT FINAL
# ============================================================

def ref_to_img(ref: Tuple[int, int], meta: Dict[int, dict]) -> np.ndarray:
    img_id, flip_state = ref
    return meta[img_id]["variants"][flip_state]


def render_preview(images: List[np.ndarray], labels: List[str], tile: int = 128) -> np.ndarray:
    cols = 4
    rows = int(math.ceil(len(images) / cols))
    margin = 16
    header_h = 28

    h = rows * (tile + header_h + margin) + margin
    w = cols * (tile + margin) + margin

    canvas = np.full((h, w, 3), 28, dtype=np.uint8)

    for idx, (img, label) in enumerate(zip(images, labels)):
        r = idx // cols
        c = idx % cols

        x = margin + c * (tile + margin)
        y = margin + r * (tile + header_h + margin) + header_h

        rgba = fit_on_canvas(img, tile)
        bgr = premultiplied_bgr(rgba)

        canvas[y:y + tile, x:x + tile] = bgr
        cv2.putText(
            canvas,
            label,
            (x, y - 8),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.46,
            (230, 230, 230),
            1,
            cv2.LINE_AA
        )

    return canvas


def export_final(
    out_dir: str,
    full_solution: Dict[str, object],
    low_solution: Dict[str, object],
    meta: Dict[int, dict]
) -> Dict[str, object]:
    ensure_dir(out_dir)

    full_refs = full_solution["best"]["refs_by_dir"]
    low_refs = low_solution["best"]["refs_by_dir"]

    exported_info = {}
    preview_images = []
    preview_labels = []

    for filename, state, direction in CANONICAL_EXPORT:
        ref = full_refs[direction] if state == "full" else low_refs[direction]
        img = ref_to_img(ref, meta)

        out_path = os.path.join(out_dir, filename)
        save_rgba(out_path, img)

        source_id, flip_state = ref
        exported_info[filename] = {
            "state": state,
            "direction": direction,
            "source_id": int(source_id),
            "flip_state": int(flip_state),
        }

        preview_images.append(img)
        preview_labels.append(f"{filename} | src={source_id} | flip={flip_state}")

    preview = render_preview(preview_images, preview_labels, tile=128)
    preview_path = os.path.join(out_dir, "_preview_final.png")
    ok = cv2.imwrite(preview_path, preview)
    if not ok:
        raise RuntimeError(f"Échec écriture preview: {preview_path}")

    report = {
        "full_solution": full_solution,
        "low_solution": low_solution,
        "exported_files": exported_info,
    }

    with open(os.path.join(out_dir, "_report.json"), "w", encoding="utf-8") as f:
        json.dump(jsonable(report), f, ensure_ascii=False, indent=2)

    return report


# ============================================================
# CONSOLE
# ============================================================

def print_state_summary(solution: Dict[str, object]) -> None:
    best = solution["best"]
    state = solution["state"]

    print(f"\n--- {state.upper()} ---")
    print(f"Score groupe: {best['total_score']:.2f}")
    print(f"Écart avec 2e solution: {solution['confidence_gap']:.2f}")
    print("Group ids:")
    for d in DIRECTIONS:
        print(f"  {d:12s} <- {solution['group_ids'][d]}")

    print(f"Garder paire bas depuis gauche ? {best['keep_bottom_left']}")
    print(f"Garder paire haut depuis gauche ? {best['keep_top_left']}")

    print("Final refs:")
    for d in DIRECTIONS:
        source_id, flip_state = best["refs_by_dir"][d]
        print(f"  {d:12s} <- source {source_id}, flip={flip_state}")

    print(
        f"Pair mean={best['pair_support_mean']:.2f}, "
        f"Base penalty mean={best['base_penalty_mean']:.2f}, "
        f"Global final={best['global_final_score']:.2f}, "
        f"Rare penalty={best['global_rare_penalty']:.2f}"
    )


# ============================================================
# MAIN
# ============================================================

def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Script 2 via mapping.json : choisit la meilleure base par paire "
            "et exporte les 8 fichiers finaux avec le bon naming."
        )
    )
    parser.add_argument("input_folder", help="Dossier contenant 1.png à 8.png")
    parser.add_argument("output_folder", help="Dossier de sortie final")
    parser.add_argument("--mapping", default=None, help="Chemin vers mapping.json (défaut: input_folder/mapping.json)")
    args = parser.parse_args()

    input_folder = os.path.abspath(args.input_folder)
    output_folder = os.path.abspath(args.output_folder)

    if not os.path.isdir(input_folder):
        raise NotADirectoryError(f"Dossier introuvable: {input_folder}")

    mapping_path = os.path.abspath(args.mapping) if args.mapping else os.path.join(input_folder, "mapping.json")
    if not os.path.isfile(mapping_path):
        raise FileNotFoundError(f"mapping.json introuvable: {mapping_path}")

    imgs = load_images(input_folder)
    mapping = load_mapping(mapping_path)
    validate_mapping(mapping)
    groups = build_group_ids(mapping)

    meta = build_meta(imgs)
    pair_metrics = build_pair_metrics(groups, imgs)

    full_solution = solve_state_group("full", groups["full"], meta, pair_metrics)
    low_solution = solve_state_group("low", groups["low"], meta, pair_metrics)

    print("\n=== RÉSULTAT ===")
    print_state_summary(full_solution)
    print_state_summary(low_solution)

    report = export_final(output_folder, full_solution, low_solution, meta)

    print("\n=== EXPORT TERMINÉ ===")
    print(f"Dossier de sortie: {output_folder}")
    for filename, state, direction in CANONICAL_EXPORT:
        info = report["exported_files"][filename]
        print(
            f"  {filename} -> {direction}, {state} | "
            f"source={info['source_id']} | flip={info['flip_state']}"
        )
    print("Rapport: _report.json")
    print("Preview: _preview_final.png")


if __name__ == "__main__":
    main()