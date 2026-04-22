#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import os
import sys
from typing import List, Dict, Tuple

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


# Conservé pour compatibilité CLI, mais plus utilisé quand la spritesheet est déjà transparente
DEFAULT_BG_RGB = (225, 25, 235)


def odd(n: int) -> int:
    return n if n % 2 == 1 else n + 1


def rgb_to_bgr(rgb: Tuple[int, int, int]) -> Tuple[int, int, int]:
    r, g, b = rgb
    return (b, g, r)


def build_border_mask(h: int, w: int, ratio: float = 0.03) -> np.ndarray:
    t = max(2, int(round(min(h, w) * ratio)))
    mask = np.zeros((h, w), dtype=np.uint8)
    mask[:t, :] = 1
    mask[-t:, :] = 1
    mask[:, :t] = 1
    mask[:, -t:] = 1
    return mask.astype(bool)


def compute_bg_metrics(
    bgr: np.ndarray,
    bg_bgr: Tuple[int, int, int]
) -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """
    Conservé pour garder la structure générale du script.
    Non utilisé quand la spritesheet source est déjà transparente.
    """
    if bgr.dtype != np.uint8:
        bgr_u8 = np.clip(bgr, 0, 255).astype(np.uint8)
    else:
        bgr_u8 = bgr

    bg_arr = np.array(bg_bgr, dtype=np.float32).reshape(1, 1, 3)
    rgb_dist = np.sqrt(np.sum((bgr.astype(np.float32) - bg_arr) ** 2, axis=2))

    hsv = cv2.cvtColor(bgr_u8, cv2.COLOR_BGR2HSV)
    hls = cv2.cvtColor(bgr_u8, cv2.COLOR_BGR2HLS)

    bg_patch = np.array([[bg_bgr]], dtype=np.uint8)
    bg_hsv = cv2.cvtColor(bg_patch, cv2.COLOR_BGR2HSV)[0, 0]
    bg_hls = cv2.cvtColor(bg_patch, cv2.COLOR_BGR2HLS)[0, 0]

    hue = hsv[:, :, 0].astype(np.int16)
    sat = hsv[:, :, 1].astype(np.int16)
    light = hls[:, :, 1].astype(np.int16)

    bg_h = int(bg_hsv[0])
    bg_s = int(bg_hsv[1])
    bg_l = int(bg_hls[1])

    hue_diff = np.abs(hue - bg_h)
    hue_diff = np.minimum(hue_diff, 180 - hue_diff)

    sat_diff = np.abs(sat - bg_s)
    light_diff = np.abs(light - bg_l)

    return rgb_dist, hue_diff, sat_diff, light_diff


def compute_magenta_like_mask(
    bgr: np.ndarray,
    bg_bgr: Tuple[int, int, int],
    rgb_threshold: float,
    hue_threshold: int,
    sat_threshold: int,
    light_threshold: int
) -> np.ndarray:
    """
    Conservé pour compatibilité structurelle.
    Non utilisé quand la spritesheet source est déjà transparente.
    """
    rgb_dist, hue_diff, sat_diff, light_diff = compute_bg_metrics(bgr, bg_bgr)

    near_rgb = rgb_dist <= rgb_threshold
    near_hsl = (
        (hue_diff <= hue_threshold) &
        (sat_diff <= sat_threshold) &
        (light_diff <= light_threshold)
    )

    return near_rgb | near_hsl


def compute_foreground_mask(
    img: np.ndarray,
    bg_bgr: Tuple[int, int, int],
    mask_rgb_threshold: float = 52.0,
    hue_threshold: int = 14,
    sat_threshold: int = 110,
    light_threshold: int = 95
) -> np.ndarray:
    if img.ndim != 3 or img.shape[2] not in (3, 4):
        raise ValueError("Image non supportée. Utilisez une image RGB ou RGBA.")

    # Les spritesheets sont maintenant déjà transparentes :
    # on ne fait plus aucune détection basée sur la couleur de fond.
    if img.shape[2] != 4:
        raise ValueError(
            "Cette version attend une spritesheet déjà transparente (RGBA)."
        )

    _ = (bg_bgr, mask_rgb_threshold, hue_threshold, sat_threshold, light_threshold)

    alpha = img[:, :, 3]
    h, w = alpha.shape[:2]
    border_mask = build_border_mask(h, w, ratio=0.03)

    border_alpha = alpha[border_mask]
    transparent_border_ratio = float(np.mean(border_alpha <= 8))

    if transparent_border_ratio >= 0.60:
        fg = alpha > 16
    else:
        fg = alpha > 8

    mask = (fg.astype(np.uint8) * 255)

    k_close = odd(max(3, int(round(min(h, w) * 0.006))))
    k_dilate = odd(max(3, int(round(min(h, w) * 0.008))))
    kernel_close = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (k_close, k_close))
    kernel_dilate = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (k_dilate, k_dilate))

    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel_close, iterations=1)
    mask = cv2.dilate(mask, kernel_dilate, iterations=1)

    return mask


def labels_to_instances(labels: np.ndarray, min_area: int) -> List[Dict[str, object]]:
    instances: List[Dict[str, object]] = []

    unique_labels = np.unique(labels)
    for label_id in unique_labels:
        if label_id <= 0:
            continue

        ys, xs = np.where(labels == label_id)
        if xs.size == 0:
            continue

        area = int(xs.size)
        x0 = int(xs.min())
        x1 = int(xs.max()) + 1
        y0 = int(ys.min())
        y1 = int(ys.max()) + 1
        w = x1 - x0
        h = y1 - y0
        bbox_area = w * h

        if area < min_area and bbox_area < min_area * 4:
            continue

        crop_mask = (labels[y0:y1, x0:x1] == label_id)

        instances.append({
            "label": int(label_id),
            "x": x0,
            "y": y0,
            "w": w,
            "h": h,
            "area": area,
            "bbox_area": bbox_area,
            "cx": float(xs.mean()),
            "cy": float(ys.mean()),
            "mask": crop_mask,
        })

    return instances


def connected_instances(
    mask: np.ndarray,
    img_shape: Tuple[int, int],
    min_area_ratio: float = 0.00015
) -> List[Dict[str, object]]:
    h, w = img_shape[:2]
    min_area = max(64, int(round(h * w * min_area_ratio)))
    num_labels, labels, _stats, _centroids = cv2.connectedComponentsWithStats(mask, connectivity=8)
    if num_labels <= 1:
        return []
    return labels_to_instances(labels, min_area)


def watershed_instances(
    mask: np.ndarray,
    img: np.ndarray,
    min_area_ratio: float = 0.00015
) -> List[Dict[str, object]]:
    h, w = mask.shape[:2]
    min_area = max(64, int(round(h * w * min_area_ratio)))

    mask_u8 = (mask > 0).astype(np.uint8) * 255
    if np.count_nonzero(mask_u8) == 0:
        return []

    dist = cv2.distanceTransform(mask_u8, cv2.DIST_L2, 5)
    dist = cv2.GaussianBlur(dist, (0, 0), 1.0)

    peak_k = odd(max(3, int(round(min(h, w) * 0.02))))
    peak_kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (peak_k, peak_k))
    local_max = dist >= cv2.dilate(dist, peak_kernel)
    local_max &= dist > max(2.0, 0.22 * float(dist.max()))

    seeds = (local_max.astype(np.uint8) * 255)
    small_k = odd(max(3, int(round(min(h, w) * 0.004))))
    small_kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (small_k, small_k))
    seeds = cv2.morphologyEx(seeds, cv2.MORPH_OPEN, small_kernel, iterations=1)
    seeds = cv2.dilate(seeds, small_kernel, iterations=1)

    num_markers, markers = cv2.connectedComponents(seeds)
    if num_markers <= 1:
        return []

    sure_bg = cv2.dilate(mask_u8, small_kernel, iterations=1)
    unknown = cv2.subtract(sure_bg, seeds)

    markers = markers + 1
    markers[unknown > 0] = 0

    if img.shape[2] == 4:
        ws_img = img[:, :, :3].copy()
    else:
        ws_img = img.copy()

    markers = cv2.watershed(ws_img, markers.astype(np.int32))

    ws_labels = np.where(markers > 1, markers - 1, 0).astype(np.int32)
    return labels_to_instances(ws_labels, min_area)


def detect_instances(mask: np.ndarray, img: np.ndarray, expected_count: int) -> List[Dict[str, object]]:
    base = connected_instances(mask, img.shape)

    if len(base) >= expected_count:
        return base

    split = watershed_instances(mask, img)

    if len(split) > len(base):
        return split

    return base


def sort_boxes_reading_order(boxes: List[Dict[str, object]]) -> List[Dict[str, object]]:
    if not boxes:
        return []

    boxes = sorted(boxes, key=lambda b: (b["cy"], b["cx"]))
    median_h = float(np.median([b["h"] for b in boxes]))
    row_threshold = max(10.0, median_h * 0.45)

    rows: List[Dict[str, object]] = []
    for box in boxes:
        placed = False
        for row in rows:
            if abs(float(box["cy"]) - float(row["cy"])) <= row_threshold:
                row["items"].append(box)
                row["cy"] = float(np.mean([float(b["cy"]) for b in row["items"]]))
                placed = True
                break
        if not placed:
            rows.append({"cy": float(box["cy"]), "items": [box]})

    rows = sorted(rows, key=lambda r: float(r["cy"]))
    ordered: List[Dict[str, object]] = []
    for row in rows:
        ordered.extend(sorted(row["items"], key=lambda b: int(b["x"])))
    return ordered


def save_debug_preview(img: np.ndarray, mask: np.ndarray, boxes: List[Dict[str, object]], out_dir: str) -> None:
    preview = img.copy()
    if preview.shape[2] == 4:
        preview_bgr = preview[:, :, :3].copy()
    else:
        preview_bgr = preview.copy()

    for idx, box in enumerate(boxes, start=1):
        x, y, w, h = int(box["x"]), int(box["y"]), int(box["w"]), int(box["h"])
        cv2.rectangle(preview_bgr, (x, y), (x + w, y + h), (0, 180, 0), 2)
        cv2.putText(
            preview_bgr,
            str(idx),
            (x + 4, max(18, y + 18)),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.7,
            (0, 180, 0),
            2,
            cv2.LINE_AA
        )

    cv2.imwrite(os.path.join(out_dir, "_debug_mask.png"), mask)
    cv2.imwrite(os.path.join(out_dir, "_debug_boxes.png"), preview_bgr)


def extract_sprite_rgba(
    crop: np.ndarray,
    sprite_mask: np.ndarray,
    bg_bgr: Tuple[int, int, int],
    fringe_radius: int = 1,
    key_rgb_threshold: float = 38.0,
    fade_width: float = 52.0,
    hue_threshold: int = 14,
    sat_threshold: int = 110,
    light_threshold: int = 95,
    alpha_cut: int = 8,
    core_erode: int = 1
) -> np.ndarray:
    """
    Version pour spritesheet déjà transparente :
    - ne supprime plus le background par couleur
    - conserve la séparation par mask pour éviter de prendre un sprite voisin
    - conserve l'alpha et les couleurs d'origine
    """
    if crop.ndim != 3 or crop.shape[2] != 4:
        raise ValueError("Le crop doit être en RGBA.")

    _ = (bg_bgr, key_rgb_threshold, fade_width, hue_threshold, sat_threshold, light_threshold, alpha_cut)

    sprite_mask = sprite_mask.astype(bool)

    rgba = crop.copy()
    base_alpha = rgba[:, :, 3].astype(np.uint8)

    refined_u8 = (sprite_mask.astype(np.uint8) * 255)

    core = sprite_mask
    if core_erode > 0:
        k_core = 2 * core_erode + 1
        ker_core = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (k_core, k_core))
        eroded = cv2.erode(refined_u8, ker_core, iterations=1) > 0
        if np.count_nonzero(eroded) >= 12:
            core = eroded

    k = 2 * fringe_radius + 1
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (k, k))
    support = cv2.dilate(refined_u8, kernel, iterations=1) > 0

    # On garde seulement la zone du sprite détecté, plus une petite marge contrôlée
    keep = support & (base_alpha > 0)

    out = rgba.copy()
    out[:, :, 3] = np.where(keep, base_alpha, 0).astype(np.uint8)
    out[out[:, :, 3] == 0] = 0

    # Assure que le coeur du sprite garde l'alpha d'origine intégralement
    out[:, :, 3] = np.where(core, base_alpha, out[:, :, 3]).astype(np.uint8)

    return out


def create_uniform_canvas(crop_rgba: np.ndarray, target_w: int, target_h: int) -> np.ndarray:
    crop_h, crop_w = crop_rgba.shape[:2]

    if crop_rgba.ndim != 3 or crop_rgba.shape[2] != 4:
        raise ValueError("Le sprite doit être en RGBA.")

    if crop_w > target_w or crop_h > target_h:
        raise ValueError("Le sprite est plus grand que la taille cible.")

    canvas = np.zeros((target_h, target_w, 4), dtype=crop_rgba.dtype)

    x_off = (target_w - crop_w) // 2
    y_off = (target_h - crop_h) // 2

    canvas[y_off:y_off + crop_h, x_off:x_off + crop_w] = crop_rgba
    return canvas


def split_spritesheet(
    input_path: str,
    out_dir: str,
    expected_count: int,
    debug: bool,
    bg_rgb: Tuple[int, int, int],
    fringe_radius: int,
    bbox_pad: int,
    key_rgb_threshold: float,
    fade_width: float,
    hue_threshold: int,
    sat_threshold: int,
    light_threshold: int,
    alpha_cut: int,
    core_erode: int
) -> int:
    img = cv2.imread(input_path, cv2.IMREAD_UNCHANGED)
    if img is None:
        raise FileNotFoundError(f"Impossible d'ouvrir l'image: {input_path}")

    if img.ndim != 3 or img.shape[2] != 4:
        raise ValueError("Image non supportée. Utilisez une image RGBA déjà transparente.")

    img_h, img_w = img.shape[:2]
    bg_bgr = rgb_to_bgr(bg_rgb)

    os.makedirs(out_dir, exist_ok=True)
    flipped_dir = os.path.join(out_dir, "flipped")
    os.makedirs(flipped_dir, exist_ok=True)

    mask = compute_foreground_mask(
        img,
        bg_bgr=bg_bgr,
        mask_rgb_threshold=max(key_rgb_threshold + 8.0, 48.0),
        hue_threshold=hue_threshold,
        sat_threshold=sat_threshold,
        light_threshold=light_threshold
    )

    instances = detect_instances(mask, img, expected_count)

    if not instances:
        raise RuntimeError("Aucun sprite détecté. Vérifiez le canal alpha de l'image source.")

    if len(instances) > expected_count:
        instances = sorted(
            instances,
            key=lambda b: (int(b["area"]), int(b["bbox_area"])),
            reverse=True
        )[:expected_count]

    instances = sort_boxes_reading_order(instances)

    prepared: List[Tuple[np.ndarray, Dict[str, object]]] = []
    common_w = 0
    common_h = 0

    for inst in instances:
        x = int(inst["x"])
        y = int(inst["y"])
        bw = int(inst["w"])
        bh = int(inst["h"])

        x0 = max(0, x - bbox_pad)
        y0 = max(0, y - bbox_pad)
        x1 = min(img_w, x + bw + bbox_pad)
        y1 = min(img_h, y + bh + bbox_pad)

        crop = img[y0:y1, x0:x1]

        local_mask = np.zeros((y1 - y0, x1 - x0), dtype=bool)
        mx0 = x - x0
        my0 = y - y0
        local_mask[my0:my0 + bh, mx0:mx0 + bw] = inst["mask"]

        sprite_rgba = extract_sprite_rgba(
            crop,
            local_mask,
            bg_bgr=bg_bgr,
            fringe_radius=fringe_radius,
            key_rgb_threshold=key_rgb_threshold,
            fade_width=fade_width,
            hue_threshold=hue_threshold,
            sat_threshold=sat_threshold,
            light_threshold=light_threshold,
            alpha_cut=alpha_cut,
            core_erode=core_erode
        )

        prepared.append((sprite_rgba, inst))
        common_w = max(common_w, sprite_rgba.shape[1])
        common_h = max(common_h, sprite_rgba.shape[0])

    count = 0
    for idx, (sprite_rgba, _inst) in enumerate(prepared, start=1):
        uniform_crop = create_uniform_canvas(sprite_rgba, common_w, common_h)

        out_path = os.path.join(out_dir, f"{idx}.png")
        ok = cv2.imwrite(out_path, uniform_crop)
        if not ok:
            raise RuntimeError(f"Échec lors de l'écriture de {out_path}")

        flipped = cv2.flip(uniform_crop, 1)
        flip_out_path = os.path.join(flipped_dir, f"flip-{idx}.png")
        ok_flip = cv2.imwrite(flip_out_path, flipped)
        if not ok_flip:
            raise RuntimeError(f"Échec lors de l'écriture de {flip_out_path}")

        count += 1

    if debug:
        save_debug_preview(img, mask, instances, out_dir)

    return count


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Découpe automatiquement une spritesheet en sprites PNG déjà transparents, sans chroma key."
    )
    parser.add_argument("input", help="Chemin vers la spritesheet source")
    parser.add_argument("output", nargs="?", default="output_sprites", help="Dossier de sortie")
    parser.add_argument("--count", type=int, default=8, help="Nombre de sprites attendu (défaut: 8)")
    parser.add_argument("--debug", action="store_true", help="Enregistre aussi un masque et une image de debug")

    # Conservés pour compatibilité avec les anciennes commandes, mais plus utilisés pour supprimer le fond
    parser.add_argument("--bg-r", type=int, default=DEFAULT_BG_RGB[0], help="Compatibilité CLI (ignoré si la source est déjà transparente)")
    parser.add_argument("--bg-g", type=int, default=DEFAULT_BG_RGB[1], help="Compatibilité CLI (ignoré si la source est déjà transparente)")
    parser.add_argument("--bg-b", type=int, default=DEFAULT_BG_RGB[2], help="Compatibilité CLI (ignoré si la source est déjà transparente)")

    parser.add_argument("--fringe-radius", type=int, default=1, help="Rayon de support autour du mask (défaut: 1)")
    parser.add_argument("--bbox-pad", type=int, default=2, help="Marge autour du sprite avant extraction (défaut: 2)")
    parser.add_argument("--key-rgb-threshold", type=float, default=38.0, help="Compatibilité CLI (ignoré pour le fond, conservé dans la signature)")
    parser.add_argument("--fade-width", type=float, default=52.0, help="Compatibilité CLI (ignoré pour le fond, conservé dans la signature)")
    parser.add_argument("--hue-threshold", type=int, default=14, help="Compatibilité CLI (ignoré pour le fond, conservé dans la signature)")
    parser.add_argument("--sat-threshold", type=int, default=110, help="Compatibilité CLI (ignoré pour le fond, conservé dans la signature)")
    parser.add_argument("--light-threshold", type=int, default=95, help="Compatibilité CLI (ignoré pour le fond, conservé dans la signature)")
    parser.add_argument("--alpha-cut", type=int, default=8, help="Compatibilité CLI (ignoré pour le fond, conservé dans la signature)")
    parser.add_argument("--core-erode", type=int, default=1, help="Érosion du noyau sûr du sprite (défaut: 1)")

    args = parser.parse_args()

    try:
        found = split_spritesheet(
            args.input,
            args.output,
            args.count,
            args.debug,
            (args.bg_r, args.bg_g, args.bg_b),
            args.fringe_radius,
            args.bbox_pad,
            args.key_rgb_threshold,
            args.fade_width,
            args.hue_threshold,
            args.sat_threshold,
            args.light_threshold,
            args.alpha_cut,
            args.core_erode
        )
        print(f"Terminé. {found} sprite(s) exporté(s) dans: {os.path.abspath(args.output)}")
        print(f"Sprites retournés horizontalement dans: {os.path.abspath(os.path.join(args.output, 'flipped'))}")
        if found != args.count:
            print(f"Attention: {args.count} sprite(s) attendu(s), mais {found} exporté(s).")
            sys.exit(2)
    except Exception as exc:
        print(f"Erreur: {exc}")
        sys.exit(1)


if __name__ == "__main__":
    main()
