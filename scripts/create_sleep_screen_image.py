#!/usr/bin/env python3
"""
Convert images to sleep screen format for Papyrix e-paper display.

Converts PNG, JPG, BMP images to grayscale BMP format compatible
with the Papyrix firmware. Supports 2, 4, or 8 bit output depth.

Usage:
    python3 create_sleep_screen_image.py <input> <output> [options]

Examples:
    python3 create_sleep_screen_image.py photo.jpg sleep.bmp
    python3 create_sleep_screen_image.py photo.png sleep.bmp --dither --bits 8
    python3 create_sleep_screen_image.py photo.jpg sleep.bmp --orientation landscape
"""

import argparse
import struct
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow is required. Install with: pip install Pillow")
    sys.exit(1)

# Display dimensions
PORTRAIT_WIDTH = 480
PORTRAIT_HEIGHT = 800
LANDSCAPE_WIDTH = 800
LANDSCAPE_HEIGHT = 480


def create_grayscale_palette(bits: int) -> list[tuple[int, int, int]]:
    """Create grayscale palette for given bit depth."""
    levels = 2 ** bits
    step = 255 // (levels - 1)
    return [(i * step, i * step, i * step) for i in range(levels)]


def quantize_to_levels(gray: int, bits: int) -> int:
    """Quantize grayscale value to target bit depth levels."""
    levels = 2 ** bits
    # Map 0-255 to 0-(levels-1)
    return min(levels - 1, (gray * levels) // 256)


def floyd_steinberg_dither(img: Image.Image, bits: int) -> Image.Image:
    """Apply Floyd-Steinberg dithering to grayscale image."""
    pixels = list(img.getdata())
    width, height = img.size
    levels = 2 ** bits
    step = 255 // (levels - 1)

    for y in range(height):
        for x in range(width):
            idx = y * width + x
            old_pixel = pixels[idx]

            # Quantize
            level = min(levels - 1, (old_pixel * levels) // 256)
            new_pixel = level * step
            pixels[idx] = new_pixel

            # Calculate error
            error = old_pixel - new_pixel

            # Distribute error to neighbors
            if x + 1 < width:
                pixels[idx + 1] = max(0, min(255, pixels[idx + 1] + error * 7 // 16))
            if y + 1 < height:
                if x > 0:
                    pixels[idx + width - 1] = max(0, min(255, pixels[idx + width - 1] + error * 3 // 16))
                pixels[idx + width] = max(0, min(255, pixels[idx + width] + error * 5 // 16))
                if x + 1 < width:
                    pixels[idx + width + 1] = max(0, min(255, pixels[idx + width + 1] + error // 16))

    result = Image.new('L', img.size)
    result.putdata(pixels)
    return result


def resize_image(img: Image.Image, target_width: int, target_height: int, fit: str) -> Image.Image:
    """Resize image according to fit mode."""
    src_ratio = img.width / img.height
    dst_ratio = target_width / target_height

    if fit == 'stretch':
        return img.resize((target_width, target_height), Image.Resampling.LANCZOS)

    elif fit == 'cover':
        # Scale to cover entire target, then crop center
        if src_ratio > dst_ratio:
            # Image is wider, scale by height
            new_height = target_height
            new_width = int(img.width * target_height / img.height)
        else:
            # Image is taller, scale by width
            new_width = target_width
            new_height = int(img.height * target_width / img.width)

        resized = img.resize((new_width, new_height), Image.Resampling.LANCZOS)

        # Crop center
        left = (new_width - target_width) // 2
        top = (new_height - target_height) // 2
        return resized.crop((left, top, left + target_width, top + target_height))

    else:  # contain (default)
        # Scale to fit within target, center on white background
        if src_ratio > dst_ratio:
            # Image is wider, constrain by width
            new_width = target_width
            new_height = int(img.height * target_width / img.width)
        else:
            # Image is taller, constrain by height
            new_height = target_height
            new_width = int(img.width * target_height / img.height)

        resized = img.resize((new_width, new_height), Image.Resampling.LANCZOS)

        # Create white background and paste centered
        result = Image.new('L', (target_width, target_height), 255)
        x = (target_width - new_width) // 2
        y = (target_height - new_height) // 2
        result.paste(resized, (x, y))
        return result


def write_bmp(img: Image.Image, output_path: Path, bits: int):
    """Write image as indexed grayscale BMP."""
    width, height = img.size
    levels = 2 ** bits

    # Calculate row bytes with 4-byte alignment
    pixels_per_byte = 8 // bits
    row_bytes = (width + pixels_per_byte - 1) // pixels_per_byte
    row_bytes_padded = (row_bytes + 3) & ~3

    # Palette size (4 bytes per color: B, G, R, reserved)
    palette_size = levels * 4

    # File structure sizes
    file_header_size = 14
    dib_header_size = 40
    pixel_data_offset = file_header_size + dib_header_size + palette_size
    pixel_data_size = row_bytes_padded * height
    file_size = pixel_data_offset + pixel_data_size

    with open(output_path, 'wb') as f:
        # BMP File Header (14 bytes)
        f.write(b'BM')  # Magic number
        f.write(struct.pack('<I', file_size))  # File size
        f.write(struct.pack('<HH', 0, 0))  # Reserved
        f.write(struct.pack('<I', pixel_data_offset))  # Pixel data offset

        # DIB Header (BITMAPINFOHEADER - 40 bytes)
        f.write(struct.pack('<I', dib_header_size))  # Header size
        f.write(struct.pack('<i', width))  # Width
        f.write(struct.pack('<i', height))  # Height (positive = bottom-up)
        f.write(struct.pack('<H', 1))  # Planes
        f.write(struct.pack('<H', bits))  # Bits per pixel
        f.write(struct.pack('<I', 0))  # Compression (BI_RGB)
        f.write(struct.pack('<I', pixel_data_size))  # Image size
        f.write(struct.pack('<i', 2835))  # X pixels per meter (72 DPI)
        f.write(struct.pack('<i', 2835))  # Y pixels per meter (72 DPI)
        f.write(struct.pack('<I', levels))  # Colors used
        f.write(struct.pack('<I', levels))  # Important colors

        # Write grayscale palette
        palette = create_grayscale_palette(bits)
        for r, g, b in palette:
            f.write(struct.pack('BBBB', b, g, r, 0))  # BGRA format

        # Write pixel data (bottom-up)
        pixels = list(img.getdata())

        for y in range(height - 1, -1, -1):  # Bottom to top
            row_data = bytearray(row_bytes_padded)
            byte_idx = 0
            bit_pos = 8 - bits

            for x in range(width):
                idx = y * width + x
                gray = pixels[idx]
                level = quantize_to_levels(gray, bits)

                row_data[byte_idx] |= (level << bit_pos)
                bit_pos -= bits

                if bit_pos < 0:
                    byte_idx += 1
                    bit_pos = 8 - bits

            f.write(row_data)


def convert_image(input_path: Path, output_path: Path, orientation: str,
                  bits: int, dither: bool, fit: str):
    """Convert image to sleep screen format."""
    # Load image
    img = Image.open(input_path)

    # Convert to grayscale
    if img.mode != 'L':
        img = img.convert('L')

    # Determine target dimensions
    if orientation == 'landscape':
        target_width = LANDSCAPE_WIDTH
        target_height = LANDSCAPE_HEIGHT
    else:
        target_width = PORTRAIT_WIDTH
        target_height = PORTRAIT_HEIGHT

    # Resize to target dimensions
    img = resize_image(img, target_width, target_height, fit)

    # Apply dithering if requested
    if dither:
        img = floyd_steinberg_dither(img, bits)

    # Write output BMP
    write_bmp(img, output_path, bits)

    print(f"Created: {output_path}")
    print(f"  Size: {target_width}x{target_height}")
    print(f"  Depth: {bits}-bit ({2**bits} levels)")
    print(f"  Dithering: {'enabled' if dither else 'disabled'}")


def main():
    parser = argparse.ArgumentParser(
        description='Convert images to Papyrix sleep screen format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  %(prog)s photo.jpg sleep.bmp
  %(prog)s photo.png sleep.bmp --dither
  %(prog)s photo.jpg sleep.bmp --bits 8 --orientation landscape
'''
    )

    parser.add_argument('input', type=Path, help='Input image (PNG, JPG, BMP)')
    parser.add_argument('output', type=Path, help='Output BMP file')
    parser.add_argument('--orientation', choices=['portrait', 'landscape'],
                        default='portrait', help='Screen orientation (default: portrait)')
    parser.add_argument('--bits', type=int, choices=[2, 4, 8],
                        default=4, help='Output bit depth (default: 4)')
    parser.add_argument('--dither', action='store_true',
                        help='Enable Floyd-Steinberg dithering')
    parser.add_argument('--fit', choices=['contain', 'cover', 'stretch'],
                        default='contain', help='Resize mode (default: contain)')

    args = parser.parse_args()

    if not args.input.exists():
        print(f"Error: Input file not found: {args.input}")
        sys.exit(1)

    convert_image(args.input, args.output, args.orientation,
                  args.bits, args.dither, args.fit)


if __name__ == '__main__':
    main()
