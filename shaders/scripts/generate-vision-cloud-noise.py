#!/usr/bin/env python3
"""
Generate a tileable grayscale texture for the vision-cloud shader.

Output:
    assets/images/shaders/visionclouds/cloud-groups-noise.png
"""

from pathlib import Path

import numpy as np
from PIL import Image


SIZE = 1024
SEED = 20260405


def spectral_noise(rng: np.random.Generator, power: float, low_cut: float, high_cut: float) -> np.ndarray:
    """Return tileable noise shaped in frequency space."""
    white = rng.standard_normal((SIZE, SIZE), dtype=np.float64)
    spectrum = np.fft.rfft2(white)

    ky = np.fft.fftfreq(SIZE)[:, None]
    kx = np.fft.rfftfreq(SIZE)[None, :]
    k = np.sqrt((kx * kx) + (ky * ky))
    k[0, 0] = 1e-6

    filt = np.power(k, -power)
    filt *= (1.0 - np.exp(-np.square(k / max(low_cut, 1e-6))))
    filt *= np.exp(-np.square(k / max(high_cut, 1e-6)))

    out = np.fft.irfft2(spectrum * filt, s=(SIZE, SIZE)).real
    out -= out.min()
    out /= max(out.max(), 1e-8)
    return out


def blur(img: np.ndarray, sigma_x: float, sigma_y: float) -> np.ndarray:
    """Tileable anisotropic Gaussian blur in Fourier space."""
    ky = np.fft.fftfreq(SIZE)[:, None]
    kx = np.fft.rfftfreq(SIZE)[None, :]
    gauss = np.exp(-2.0 * np.pi * np.pi * ((sigma_x * sigma_x) * (kx * kx) + (sigma_y * sigma_y) * (ky * ky)))

    out = np.fft.irfft2(np.fft.rfft2(img) * gauss, s=(SIZE, SIZE)).real
    out -= out.min()
    out /= max(out.max(), 1e-8)
    return out


def main() -> None:
    rng = np.random.default_rng(SEED)

    macro = spectral_noise(rng, power=2.9, low_cut=0.0014, high_cut=0.028)
    shape = spectral_noise(rng, power=1.8, low_cut=0.0030, high_cut=0.094)
    detail = spectral_noise(rng, power=1.05, low_cut=0.0090, high_cut=0.220)

    shape = blur(shape, sigma_x=4.0, sigma_y=1.6)
    cluster = np.clip((macro - 0.40) * 2.4, 0.0, 1.0)
    cluster = blur(cluster, sigma_x=1.8, sigma_y=1.8)

    body = np.clip((shape - 0.36) * 1.95, 0.0, 1.0)
    body = body * (0.50 + 0.50 * cluster)
    edge = np.clip((detail - 0.32) * 1.55, 0.0, 1.0)

    cloud = np.clip(body * (0.72 + 0.28 * edge), 0.0, 1.0)
    cloud = np.clip((cloud - 0.09) * 1.58, 0.0, 1.0)
    cloud = cloud ** 0.90
    cloud = np.clip((cloud * 0.95) + 0.03, 0.0, 1.0)

    img = (cloud * 255.0).astype(np.uint8)
    rgb = np.dstack([img, img, img])

    out_path = Path("assets/images/shaders/visionclouds/cloud-groups-noise.png")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(rgb, mode="RGB").save(out_path, optimize=True)
    print(out_path)


if __name__ == "__main__":
    main()
