#!/usr/bin/env python3
"""
Generate seamless ocean textures for the SeaTyrants ocean shader.

Outputs (in assets/images by default):
- tile-water-base.png      (1024x1024, RGBA)
- tile-water-detail.png    (512x512, RGBA)
- tile-caustic.png         (512x512, RGBA, grayscale in RGB)
- tile-foam-streaks.png    (512x512, RGBA, alpha carries streak mask)
- water-macro.png          (2048x2048, RGBA, soft macro variation + packed depth in alpha)
- tile-water-depth.png     (1024x1024, RGBA, grayscale depth: dark=shallow, bright=deep)
"""

from __future__ import annotations

from pathlib import Path
import argparse
import math

import numpy as np
from PIL import Image


def saturate(x: np.ndarray) -> np.ndarray:
    return np.clip(x, 0.0, 1.0)


def normalize01(x: np.ndarray) -> np.ndarray:
    mn = float(x.min())
    mx = float(x.max())
    if mx - mn < 1e-12:
        return np.zeros_like(x, dtype=np.float32)
    return ((x - mn) / (mx - mn)).astype(np.float32)


def spectral_noise_periodic(h: int, w: int, beta: float, rng: np.random.Generator) -> np.ndarray:
    """
    Periodic (tileable) 2D noise generated in frequency domain using irfft2.
    beta controls roughness: higher beta => smoother/low-frequency dominance.
    """
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.rfftfreq(w)[None, :]
    f2 = fx * fx + fy * fy
    amp = np.power(f2 + 1e-9, -0.5 * beta)
    amp[0, 0] = 0.0

    real = rng.standard_normal((h, w // 2 + 1), dtype=np.float32)
    imag = rng.standard_normal((h, w // 2 + 1), dtype=np.float32)
    spectrum = (real + 1j * imag) * amp

    field = np.fft.irfft2(spectrum, s=(h, w)).astype(np.float32)
    return normalize01(field)


def make_uv(h: int, w: int) -> tuple[np.ndarray, np.ndarray]:
    y = np.arange(h, dtype=np.float32) / float(h)
    x = np.arange(w, dtype=np.float32) / float(w)
    xv, yv = np.meshgrid(x, y)
    return xv.astype(np.float32), yv.astype(np.float32)


def save_rgba(path: Path, r: np.ndarray, g: np.ndarray, b: np.ndarray, a: np.ndarray) -> None:
    rgba = np.stack(
        [
            (saturate(r) * 255.0).astype(np.uint8),
            (saturate(g) * 255.0).astype(np.uint8),
            (saturate(b) * 255.0).astype(np.uint8),
            (saturate(a) * 255.0).astype(np.uint8),
        ],
        axis=-1,
    )
    Image.fromarray(rgba, mode="RGBA").save(path)


def smoothstep(edge0: float, edge1: float, x: np.ndarray) -> np.ndarray:
    t = saturate((x - edge0) / max(edge1 - edge0, 1e-8))
    return t * t * (3.0 - 2.0 * t)


def periodic_blur_cross(x: np.ndarray, iterations: int = 1) -> np.ndarray:
    out = x.astype(np.float32)
    for _ in range(max(iterations, 0)):
        out = (
            out
            + np.roll(out, 1, axis=0)
            + np.roll(out, -1, axis=0)
            + np.roll(out, 1, axis=1)
            + np.roll(out, -1, axis=1)
        ) * 0.2
    return out.astype(np.float32)


def remap_rgb_to_palette(
    rgb: np.ndarray,
    deep: np.ndarray,
    mid: np.ndarray,
    light: np.ndarray,
    *,
    brightness_target: float = 1.0,
    saturation: float = 1.0,
) -> np.ndarray:
    """
    Recolor an RGB texture while preserving original luminance detail/noise.
    """
    lum = np.clip(
        rgb[..., 0] * 0.299 + rgb[..., 1] * 0.587 + rgb[..., 2] * 0.114,
        0.0,
        1.0,
    )
    t1 = smoothstep(0.12, 0.72, lum)
    t2 = smoothstep(0.56, 1.00, lum)

    color = deep[None, None, :] * (1.0 - t1[..., None]) + mid[None, None, :] * t1[..., None]
    color = color * (1.0 - t2[..., None]) + light[None, None, :] * t2[..., None]

    # Keep local micro-variation from original texture.
    lum_safe = np.maximum(lum[..., None], 1e-4)
    variation = saturate(rgb / lum_safe)
    out = color * (0.90 + 0.22 * variation)

    # Lift highlights a bit to keep readability on warm palettes.
    hi = smoothstep(0.62, 1.0, lum)
    out = out + hi[..., None] * 0.045

    # Match global luminance to source texture so variants do not look too dark.
    src_l = float(lum.mean())
    out_l = float((out[..., 0] * 0.299 + out[..., 1] * 0.587 + out[..., 2] * 0.114).mean())
    if out_l > 1e-6:
        gain = np.clip((src_l * brightness_target) / out_l, 0.75, 1.90)
        out = out * gain

    # Optional saturation tweak per palette.
    out_lum = out[..., 0] * 0.299 + out[..., 1] * 0.587 + out[..., 2] * 0.114
    out = out_lum[..., None] + (out - out_lum[..., None]) * saturation

    return saturate(out)


def generate_water_base(path: Path, rng: np.random.Generator) -> None:
    h = w = 1024
    x, y = make_uv(h, w)

    n_big = spectral_noise_periodic(h, w, beta=2.3, rng=rng)
    n_mid = spectral_noise_periodic(h, w, beta=1.7, rng=rng)
    n_small = spectral_noise_periodic(h, w, beta=1.1, rng=rng)

    flow = 0.5 + 0.5 * np.sin(
        2.0 * math.pi * (x * 2.19 + y * 1.63 + (n_mid - 0.5) * 0.22)
    )

    l = 0.52 * n_big + 0.28 * n_mid + 0.12 * n_small + 0.08 * flow
    l = normalize01(l)
    l = 0.18 + l * 0.72

    r = 0.015 + l * 0.070 + (n_small - 0.5) * 0.012
    g = 0.155 + l * 0.255 + (n_mid - 0.5) * 0.018
    b = 0.295 + l * 0.365 + (n_big - 0.5) * 0.022

    save_rgba(path, r, g, b, np.ones_like(l, dtype=np.float32))


def generate_water_detail(path: Path, rng: np.random.Generator) -> None:
    h = w = 512
    x, y = make_uv(h, w)

    warp_a = spectral_noise_periodic(h, w, beta=1.8, rng=rng)
    warp_b = spectral_noise_periodic(h, w, beta=1.4, rng=rng)
    u = x + (warp_a - 0.5) * 0.07
    v = y + (warp_b - 0.5) * 0.07

    p = np.zeros((h, w), dtype=np.float32)
    params = [
        (17.3, 0.32, 1.00, 0.17),
        (23.9, -0.84, 0.78, 0.61),
        (31.1, 1.18, 0.62, 1.27),
        (39.7, -0.19, 0.46, 2.04),
    ]
    for freq, angle, weight, phase in params:
        ca = math.cos(angle)
        sa = math.sin(angle)
        q = ca * u + sa * v
        r = -sa * u + ca * v
        p += weight * np.sin(2.0 * math.pi * (q * freq + 0.21 * np.sin(2.0 * math.pi * r * (freq * 0.21) + phase)))

    p = normalize01(p)
    micro = np.power(saturate((p - 0.44) * 1.95), 1.15)
    soft = spectral_noise_periodic(h, w, beta=1.05, rng=rng)
    detail = normalize01(micro * 0.74 + soft * 0.26)

    r = 0.040 + detail * 0.060
    g = 0.255 + detail * 0.205
    b = 0.430 + detail * 0.255

    save_rgba(path, r, g, b, np.ones_like(detail, dtype=np.float32))


def write_water_color_variants(out_dir: Path) -> None:
    """
    Generate multiple color variants from existing blue base/detail textures,
    preserving the exact same pattern/noise.
    """
    base_blue_path = out_dir / "tile-water-base.png"
    detail_blue_path = out_dir / "tile-water-detail.png"

    base_img = np.asarray(Image.open(base_blue_path).convert("RGBA"), dtype=np.float32) / 255.0
    detail_img = np.asarray(Image.open(detail_blue_path).convert("RGBA"), dtype=np.float32) / 255.0

    base_rgb = base_img[..., :3]
    detail_rgb = detail_img[..., :3]
    base_a = base_img[..., 3]
    detail_a = detail_img[..., 3]

    # Brighter palettes to keep the same readability as blue water.
    green_deep = np.array([0.052, 0.205, 0.152], dtype=np.float32)
    green_mid = np.array([0.108, 0.430, 0.315], dtype=np.float32)
    green_light = np.array([0.200, 0.610, 0.455], dtype=np.float32)

    brown_deep = np.array([0.165, 0.122, 0.072], dtype=np.float32)
    brown_mid = np.array([0.338, 0.248, 0.138], dtype=np.float32)
    brown_light = np.array([0.535, 0.405, 0.230], dtype=np.float32)

    amber_deep = np.array([0.205, 0.145, 0.060], dtype=np.float32)
    amber_mid = np.array([0.435, 0.325, 0.108], dtype=np.float32)
    amber_light = np.array([0.680, 0.550, 0.190], dtype=np.float32)

    yellow_deep = np.array([0.230, 0.190, 0.055], dtype=np.float32)
    yellow_mid = np.array([0.500, 0.430, 0.120], dtype=np.float32)
    yellow_light = np.array([0.760, 0.700, 0.250], dtype=np.float32)

    orange_deep = np.array([0.240, 0.115, 0.050], dtype=np.float32)
    orange_mid = np.array([0.520, 0.250, 0.090], dtype=np.float32)
    orange_light = np.array([0.790, 0.440, 0.170], dtype=np.float32)

    lime_deep = np.array([0.145, 0.215, 0.055], dtype=np.float32)
    lime_mid = np.array([0.305, 0.455, 0.115], dtype=np.float32)
    lime_light = np.array([0.515, 0.705, 0.225], dtype=np.float32)

    teal_deep = np.array([0.030, 0.175, 0.145], dtype=np.float32)
    teal_mid = np.array([0.070, 0.385, 0.315], dtype=np.float32)
    teal_light = np.array([0.145, 0.610, 0.515], dtype=np.float32)

    turquoise_deep = np.array([0.040, 0.205, 0.190], dtype=np.float32)
    turquoise_mid = np.array([0.090, 0.445, 0.415], dtype=np.float32)
    turquoise_light = np.array([0.180, 0.670, 0.625], dtype=np.float32)

    cyan_deep = np.array([0.050, 0.220, 0.260], dtype=np.float32)
    cyan_mid = np.array([0.100, 0.450, 0.520], dtype=np.float32)
    cyan_light = np.array([0.210, 0.680, 0.760], dtype=np.float32)

    jade_deep = np.array([0.045, 0.185, 0.115], dtype=np.float32)
    jade_mid = np.array([0.090, 0.395, 0.245], dtype=np.float32)
    jade_light = np.array([0.180, 0.620, 0.405], dtype=np.float32)

    purple_deep = np.array([0.100, 0.060, 0.185], dtype=np.float32)
    purple_mid = np.array([0.235, 0.125, 0.400], dtype=np.float32)
    purple_light = np.array([0.420, 0.250, 0.645], dtype=np.float32)

    violet_deep = np.array([0.130, 0.085, 0.225], dtype=np.float32)
    violet_mid = np.array([0.290, 0.175, 0.465], dtype=np.float32)
    violet_light = np.array([0.485, 0.320, 0.705], dtype=np.float32)

    magenta_deep = np.array([0.185, 0.055, 0.180], dtype=np.float32)
    magenta_mid = np.array([0.410, 0.120, 0.395], dtype=np.float32)
    magenta_light = np.array([0.650, 0.235, 0.620], dtype=np.float32)

    pink_deep = np.array([0.235, 0.105, 0.165], dtype=np.float32)
    pink_mid = np.array([0.495, 0.220, 0.335], dtype=np.float32)
    pink_light = np.array([0.740, 0.375, 0.520], dtype=np.float32)

    coral_deep = np.array([0.260, 0.115, 0.095], dtype=np.float32)
    coral_mid = np.array([0.545, 0.265, 0.205], dtype=np.float32)
    coral_light = np.array([0.815, 0.455, 0.345], dtype=np.float32)

    red_deep = np.array([0.205, 0.068, 0.068], dtype=np.float32)
    red_mid = np.array([0.435, 0.132, 0.132], dtype=np.float32)
    red_light = np.array([0.670, 0.230, 0.230], dtype=np.float32)

    base_green = remap_rgb_to_palette(base_rgb, green_deep, green_mid, green_light, brightness_target=1.02, saturation=1.02)
    detail_green = remap_rgb_to_palette(detail_rgb, green_deep, green_mid, green_light, brightness_target=1.02, saturation=1.02)
    base_brown = remap_rgb_to_palette(base_rgb, brown_deep, brown_mid, brown_light, brightness_target=1.08, saturation=1.00)
    detail_brown = remap_rgb_to_palette(detail_rgb, brown_deep, brown_mid, brown_light, brightness_target=1.08, saturation=1.00)
    base_amber = remap_rgb_to_palette(base_rgb, amber_deep, amber_mid, amber_light, brightness_target=1.10, saturation=1.04)
    detail_amber = remap_rgb_to_palette(detail_rgb, amber_deep, amber_mid, amber_light, brightness_target=1.10, saturation=1.04)
    base_yellow = remap_rgb_to_palette(base_rgb, yellow_deep, yellow_mid, yellow_light, brightness_target=1.14, saturation=1.06)
    detail_yellow = remap_rgb_to_palette(detail_rgb, yellow_deep, yellow_mid, yellow_light, brightness_target=1.14, saturation=1.06)
    base_orange = remap_rgb_to_palette(base_rgb, orange_deep, orange_mid, orange_light, brightness_target=1.12, saturation=1.05)
    detail_orange = remap_rgb_to_palette(detail_rgb, orange_deep, orange_mid, orange_light, brightness_target=1.12, saturation=1.05)
    base_lime = remap_rgb_to_palette(base_rgb, lime_deep, lime_mid, lime_light, brightness_target=1.10, saturation=1.06)
    detail_lime = remap_rgb_to_palette(detail_rgb, lime_deep, lime_mid, lime_light, brightness_target=1.10, saturation=1.06)
    base_teal = remap_rgb_to_palette(base_rgb, teal_deep, teal_mid, teal_light, brightness_target=1.06, saturation=1.03)
    detail_teal = remap_rgb_to_palette(detail_rgb, teal_deep, teal_mid, teal_light, brightness_target=1.06, saturation=1.03)
    base_turquoise = remap_rgb_to_palette(base_rgb, turquoise_deep, turquoise_mid, turquoise_light, brightness_target=1.08, saturation=1.04)
    detail_turquoise = remap_rgb_to_palette(detail_rgb, turquoise_deep, turquoise_mid, turquoise_light, brightness_target=1.08, saturation=1.04)
    base_cyan = remap_rgb_to_palette(base_rgb, cyan_deep, cyan_mid, cyan_light, brightness_target=1.09, saturation=1.04)
    detail_cyan = remap_rgb_to_palette(detail_rgb, cyan_deep, cyan_mid, cyan_light, brightness_target=1.09, saturation=1.04)
    base_jade = remap_rgb_to_palette(base_rgb, jade_deep, jade_mid, jade_light, brightness_target=1.06, saturation=1.03)
    detail_jade = remap_rgb_to_palette(detail_rgb, jade_deep, jade_mid, jade_light, brightness_target=1.06, saturation=1.03)
    base_purple = remap_rgb_to_palette(base_rgb, purple_deep, purple_mid, purple_light, brightness_target=1.07, saturation=1.01)
    detail_purple = remap_rgb_to_palette(detail_rgb, purple_deep, purple_mid, purple_light, brightness_target=1.07, saturation=1.01)
    base_violet = remap_rgb_to_palette(base_rgb, violet_deep, violet_mid, violet_light, brightness_target=1.08, saturation=1.02)
    detail_violet = remap_rgb_to_palette(detail_rgb, violet_deep, violet_mid, violet_light, brightness_target=1.08, saturation=1.02)
    base_magenta = remap_rgb_to_palette(base_rgb, magenta_deep, magenta_mid, magenta_light, brightness_target=1.08, saturation=1.04)
    detail_magenta = remap_rgb_to_palette(detail_rgb, magenta_deep, magenta_mid, magenta_light, brightness_target=1.08, saturation=1.04)
    base_pink = remap_rgb_to_palette(base_rgb, pink_deep, pink_mid, pink_light, brightness_target=1.10, saturation=1.03)
    detail_pink = remap_rgb_to_palette(detail_rgb, pink_deep, pink_mid, pink_light, brightness_target=1.10, saturation=1.03)
    base_coral = remap_rgb_to_palette(base_rgb, coral_deep, coral_mid, coral_light, brightness_target=1.11, saturation=1.04)
    detail_coral = remap_rgb_to_palette(detail_rgb, coral_deep, coral_mid, coral_light, brightness_target=1.11, saturation=1.04)
    base_red = remap_rgb_to_palette(base_rgb, red_deep, red_mid, red_light, brightness_target=1.10, saturation=0.98)
    detail_red = remap_rgb_to_palette(detail_rgb, red_deep, red_mid, red_light, brightness_target=1.10, saturation=0.98)

    save_rgba(out_dir / "tile-water-base-green.png", base_green[..., 0], base_green[..., 1], base_green[..., 2], base_a)
    save_rgba(out_dir / "tile-water-detail-green.png", detail_green[..., 0], detail_green[..., 1], detail_green[..., 2], detail_a)
    save_rgba(out_dir / "tile-water-base-brown.png", base_brown[..., 0], base_brown[..., 1], base_brown[..., 2], base_a)
    save_rgba(out_dir / "tile-water-detail-brown.png", detail_brown[..., 0], detail_brown[..., 1], detail_brown[..., 2], detail_a)
    save_rgba(out_dir / "tile-water-base-amber.png", base_amber[..., 0], base_amber[..., 1], base_amber[..., 2], base_a)
    save_rgba(out_dir / "tile-water-detail-amber.png", detail_amber[..., 0], detail_amber[..., 1], detail_amber[..., 2], detail_a)
    save_rgba(out_dir / "tile-water-base-yellow.png", base_yellow[..., 0], base_yellow[..., 1], base_yellow[..., 2], base_a)
    save_rgba(out_dir / "tile-water-detail-yellow.png", detail_yellow[..., 0], detail_yellow[..., 1], detail_yellow[..., 2], detail_a)
    save_rgba(out_dir / "tile-water-base-orange.png", base_orange[..., 0], base_orange[..., 1], base_orange[..., 2], base_a)
    save_rgba(out_dir / "tile-water-detail-orange.png", detail_orange[..., 0], detail_orange[..., 1], detail_orange[..., 2], detail_a)
    save_rgba(out_dir / "tile-water-base-lime.png", base_lime[..., 0], base_lime[..., 1], base_lime[..., 2], base_a)
    save_rgba(out_dir / "tile-water-detail-lime.png", detail_lime[..., 0], detail_lime[..., 1], detail_lime[..., 2], detail_a)
    save_rgba(out_dir / "tile-water-base-teal.png", base_teal[..., 0], base_teal[..., 1], base_teal[..., 2], base_a)
    save_rgba(out_dir / "tile-water-detail-teal.png", detail_teal[..., 0], detail_teal[..., 1], detail_teal[..., 2], detail_a)
    save_rgba(out_dir / "tile-water-base-turquoise.png", base_turquoise[..., 0], base_turquoise[..., 1], base_turquoise[..., 2], base_a)
    save_rgba(out_dir / "tile-water-detail-turquoise.png", detail_turquoise[..., 0], detail_turquoise[..., 1], detail_turquoise[..., 2], detail_a)
    save_rgba(out_dir / "tile-water-base-cyan.png", base_cyan[..., 0], base_cyan[..., 1], base_cyan[..., 2], base_a)
    save_rgba(out_dir / "tile-water-detail-cyan.png", detail_cyan[..., 0], detail_cyan[..., 1], detail_cyan[..., 2], detail_a)
    save_rgba(out_dir / "tile-water-base-jade.png", base_jade[..., 0], base_jade[..., 1], base_jade[..., 2], base_a)
    save_rgba(out_dir / "tile-water-detail-jade.png", detail_jade[..., 0], detail_jade[..., 1], detail_jade[..., 2], detail_a)
    save_rgba(out_dir / "tile-water-base-purple.png", base_purple[..., 0], base_purple[..., 1], base_purple[..., 2], base_a)
    save_rgba(out_dir / "tile-water-detail-purple.png", detail_purple[..., 0], detail_purple[..., 1], detail_purple[..., 2], detail_a)
    save_rgba(out_dir / "tile-water-base-violet.png", base_violet[..., 0], base_violet[..., 1], base_violet[..., 2], base_a)
    save_rgba(out_dir / "tile-water-detail-violet.png", detail_violet[..., 0], detail_violet[..., 1], detail_violet[..., 2], detail_a)
    save_rgba(out_dir / "tile-water-base-magenta.png", base_magenta[..., 0], base_magenta[..., 1], base_magenta[..., 2], base_a)
    save_rgba(out_dir / "tile-water-detail-magenta.png", detail_magenta[..., 0], detail_magenta[..., 1], detail_magenta[..., 2], detail_a)
    save_rgba(out_dir / "tile-water-base-pink.png", base_pink[..., 0], base_pink[..., 1], base_pink[..., 2], base_a)
    save_rgba(out_dir / "tile-water-detail-pink.png", detail_pink[..., 0], detail_pink[..., 1], detail_pink[..., 2], detail_a)
    save_rgba(out_dir / "tile-water-base-coral.png", base_coral[..., 0], base_coral[..., 1], base_coral[..., 2], base_a)
    save_rgba(out_dir / "tile-water-detail-coral.png", detail_coral[..., 0], detail_coral[..., 1], detail_coral[..., 2], detail_a)
    save_rgba(out_dir / "tile-water-base-red.png", base_red[..., 0], base_red[..., 1], base_red[..., 2], base_a)
    save_rgba(out_dir / "tile-water-detail-red.png", detail_red[..., 0], detail_red[..., 1], detail_red[..., 2], detail_a)


def worley_edge_field(h: int, w: int, points: np.ndarray) -> np.ndarray:
    x, y = make_uv(h, w)
    d1 = np.full((h, w), 1e9, dtype=np.float32)
    d2 = np.full((h, w), 1e9, dtype=np.float32)

    for px, py in points:
        dx = np.abs(x - px)
        dy = np.abs(y - py)
        dx = np.minimum(dx, 1.0 - dx)
        dy = np.minimum(dy, 1.0 - dy)
        d = np.sqrt(dx * dx + dy * dy, dtype=np.float32)

        new_d1 = np.minimum(d1, d)
        new_d2 = np.minimum(d2, np.maximum(d1, d))
        d1 = new_d1
        d2 = new_d2

    return d2 - d1


def generate_caustic(path: Path, rng: np.random.Generator) -> None:
    h = w = 512
    points_a = rng.random((64, 2), dtype=np.float32)
    points_b = rng.random((92, 2), dtype=np.float32)

    edge_a = worley_edge_field(h, w, points_a)
    edge_b = worley_edge_field(h, w, points_b)

    lines_a = np.exp(-np.square(edge_a * 210.0), dtype=np.float32)
    lines_b = np.exp(-np.square(edge_b * 260.0), dtype=np.float32)
    lines = np.maximum(lines_a * 0.80, lines_b * 0.65)

    mod = spectral_noise_periodic(h, w, beta=1.9, rng=rng)
    mod = 0.68 + 0.55 * mod
    lines *= mod

    lines = np.power(saturate(lines), 1.05)
    lines = saturate((lines - 0.06) * 1.35)

    save_rgba(path, lines, lines, lines, np.ones_like(lines, dtype=np.float32))


def generate_foam_streaks(path: Path, rng: np.random.Generator) -> None:
    h = w = 512
    seed = (rng.random((h, w), dtype=np.float32) > 0.991).astype(np.float32)

    # Build directional streaks with wrapped shifts (seamless by design).
    accum = np.zeros((h, w), dtype=np.float32)
    total_w = 0.0
    for k in range(-18, 19):
        weight = math.exp(-((k / 6.6) ** 2))
        sx = int(round(k * 2.6))
        sy = int(round(k * 0.8))
        shifted = np.roll(np.roll(seed, sy, axis=0), sx, axis=1)
        accum += shifted * weight
        total_w += weight
    accum /= max(total_w, 1e-8)

    base_noise = spectral_noise_periodic(h, w, beta=1.2, rng=rng)
    break_noise = spectral_noise_periodic(h, w, beta=0.85, rng=rng)

    alpha = saturate((accum - 0.018) * 10.0)
    alpha *= saturate((base_noise - 0.22) * 2.2)
    alpha *= saturate((break_noise - 0.30) * 2.0)
    alpha = np.power(alpha, 0.7)
    alpha = saturate(alpha * 0.95)

    rgb = np.ones_like(alpha, dtype=np.float32)
    save_rgba(path, rgb, rgb, rgb, alpha)


def generate_macro(path: Path, rng: np.random.Generator) -> None:
    h = w = 2048
    x, y = make_uv(h, w)
    big = spectral_noise_periodic(h, w, beta=3.1, rng=rng)
    mid = spectral_noise_periodic(h, w, beta=2.2, rng=rng)
    drift = 0.5 + 0.5 * np.sin(2.0 * math.pi * (x * 0.83 - y * 0.57 + (mid - 0.5) * 0.10))

    macro = normalize01(big * 0.70 + mid * 0.20 + drift * 0.10)
    macro = 0.34 + macro * 0.36

    # Pack a large-scale pseudo-bathymetry map in alpha so the shader can
    # drive coastal tinting/foam without requiring an extra sampler binding.
    depth_alpha = build_water_depth_field(h, w, rng)
    depth_alpha = saturate(depth_alpha * 0.92 + 0.04)

    save_rgba(path, macro, macro, macro, depth_alpha)


def build_water_depth_field(h: int, w: int, rng: np.random.Generator) -> np.ndarray:
    """
    Tileable pseudo-bathymetry map.
    dark  -> shallow water / coastal shelf
    bright -> deep ocean
    """
    x, y = make_uv(h, w)

    n_continent = spectral_noise_periodic(h, w, beta=3.35, rng=rng)
    n_mid = spectral_noise_periodic(h, w, beta=2.55, rng=rng)
    n_fine = spectral_noise_periodic(h, w, beta=1.90, rng=rng)
    warp_u = spectral_noise_periodic(h, w, beta=2.30, rng=rng)
    warp_v = spectral_noise_periodic(h, w, beta=2.20, rng=rng)

    u = np.mod(x + (warp_u - 0.5) * 0.075, 1.0).astype(np.float32)
    v = np.mod(y + (warp_v - 0.5) * 0.075, 1.0).astype(np.float32)
    ridges = 0.5 + 0.5 * np.sin(2.0 * math.pi * (u * 1.35 + v * 1.12 + (n_fine - 0.5) * 0.22))

    basin = smoothstep(0.36, 0.80, n_continent)
    shelf = smoothstep(0.28, 0.56, n_mid) * (1.0 - smoothstep(0.56, 0.86, n_mid))

    depth = (
        0.14
        + basin * 0.60
        + n_mid * 0.23
        + ridges * 0.09
        - shelf * 0.17
    )
    depth = normalize01(depth)
    depth = np.power(depth, 1.26)
    depth = periodic_blur_cross(depth, iterations=5)
    depth = normalize01(depth)
    depth = saturate(depth * 0.94 + 0.03)

    return depth


def generate_water_depth(path: Path, rng: np.random.Generator) -> None:
    h = w = 1024
    depth = build_water_depth_field(h, w, rng)
    save_rgba(path, depth, depth, depth, np.ones_like(depth, dtype=np.float32))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--out",
        type=Path,
        default=Path("assets/images"),
        help="Output directory",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=24041991,
        help="Random seed for deterministic generation",
    )
    parser.add_argument(
        "--full-pack",
        action="store_true",
        help="Also regenerate caustic/foam/macro/depth textures (default: only base+detail+color variants).",
    )
    args = parser.parse_args()

    out_dir = args.out
    out_dir.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(args.seed)

    print(f"[watergen] output: {out_dir.resolve()}")
    print(f"[watergen] seed: {args.seed}")

    generate_water_base(out_dir / "tile-water-base.png", rng)
    print("[watergen] wrote tile-water-base.png (1024x1024)")

    generate_water_detail(out_dir / "tile-water-detail.png", rng)
    print("[watergen] wrote tile-water-detail.png (512x512)")

    write_water_color_variants(out_dir)
    print("[watergen] wrote tile-water-base/detail variants: GREEN, BROWN, AMBER, YELLOW, ORANGE, LIME, TEAL, TURQUOISE, CYAN, JADE, PURPLE, VIOLET, MAGENTA, PINK, CORAL, RED")

    if args.full_pack:
        generate_caustic(out_dir / "tile-caustic.png", rng)
        print("[watergen] wrote tile-caustic.png (512x512)")

        generate_foam_streaks(out_dir / "tile-foam-streaks.png", rng)
        print("[watergen] wrote tile-foam-streaks.png (512x512)")

        generate_macro(out_dir / "water-macro.png", rng)
        print("[watergen] wrote water-macro.png (2048x2048)")

        generate_water_depth(out_dir / "tile-water-depth.png", rng)
        print("[watergen] wrote tile-water-depth.png (1024x1024)")
    else:
        print("[watergen] skipped tile-caustic/tile-foam-streaks/water-macro/tile-water-depth (use --full-pack to regenerate all)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
