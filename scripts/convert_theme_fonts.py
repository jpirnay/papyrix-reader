#!/usr/bin/env python3
"""
Convert TTF/OTF fonts to Papyrix theme format (.epdfont)

Creates a font family directory with all style variants for use with Papyrix themes.

Usage:
    python3 convert_theme_fonts.py my-font -r Regular.ttf -b Bold.ttf -i Italic.ttf -bi BoldItalic.ttf
    python3 convert_theme_fonts.py my-font -r Regular.ttf --size 16
    python3 convert_theme_fonts.py my-font -r Regular.ttf -o /path/to/sdcard/fonts/

Requirements:
    pip install freetype-py
"""

import argparse
import os
import sys
import subprocess
from pathlib import Path

# Path to fontconvert.py relative to this script
SCRIPT_DIR = Path(__file__).parent.resolve()
FONTCONVERT_PATH = SCRIPT_DIR.parent / "lib" / "EpdFont" / "scripts" / "fontconvert.py"


def convert_font(font_path: Path, output_path: Path, name: str, size: int, is_2bit: bool) -> bool:
    """Convert a single font file to .epdfont format."""
    if not font_path.exists():
        print(f"  Warning: Font file not found: {font_path}", file=sys.stderr)
        return False

    cmd = [
        sys.executable,
        str(FONTCONVERT_PATH),
        name,
        str(size),
        str(font_path),
        "--binary",
        "-o", str(output_path),
    ]

    if is_2bit:
        cmd.append("--2bit")

    print(f"  Converting: {font_path.name} -> {output_path.name}")

    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"  Error: {result.stderr}", file=sys.stderr)
            return False
        return True
    except Exception as e:
        print(f"  Error: {e}", file=sys.stderr)
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Convert TTF/OTF fonts to Papyrix theme format (.epdfont)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Convert a complete font family
  %(prog)s my-font -r MyFont-Regular.ttf -b MyFont-Bold.ttf -i MyFont-Italic.ttf -bi MyFont-BoldItalic.ttf

  # Convert only regular style
  %(prog)s my-font -r MyFont-Regular.ttf

  # Specify font size (default: 16)
  %(prog)s my-font -r MyFont-Regular.ttf --size 14

  # Output to SD card
  %(prog)s my-font -r MyFont-Regular.ttf -o /Volumes/SDCARD/fonts/

  # Use 2-bit grayscale for smoother rendering
  %(prog)s my-font -r MyFont-Regular.ttf --2bit

Output structure:
  <output>/
  └── <family-name>/
      ├── regular.epdfont
      ├── bold.epdfont
      ├── italic.epdfont
      └── bold_italic.epdfont
        """,
    )

    parser.add_argument(
        "family",
        help="Font family name (used as directory name)",
    )
    parser.add_argument(
        "-r", "--regular",
        required=True,
        help="Path to regular style TTF/OTF file",
    )
    parser.add_argument(
        "-b", "--bold",
        help="Path to bold style TTF/OTF file",
    )
    parser.add_argument(
        "-i", "--italic",
        help="Path to italic style TTF/OTF file",
    )
    parser.add_argument(
        "-bi", "--bold-italic",
        help="Path to bold-italic style TTF/OTF file",
    )
    parser.add_argument(
        "-o", "--output",
        default=".",
        help="Output directory (default: current directory)",
    )
    parser.add_argument(
        "-s", "--size",
        type=int,
        default=16,
        help="Font size in points (default: 16)",
    )
    parser.add_argument(
        "--2bit",
        dest="is_2bit",
        action="store_true",
        help="Generate 2-bit grayscale (smoother but larger)",
    )
    parser.add_argument(
        "--all-sizes",
        action="store_true",
        help="Generate all reader font sizes (14, 16, 18pt)",
    )

    args = parser.parse_args()

    # Check fontconvert.py exists
    if not FONTCONVERT_PATH.exists():
        print(f"Error: fontconvert.py not found at {FONTCONVERT_PATH}", file=sys.stderr)
        print("Make sure you're running this from the papyrix-reader repository.", file=sys.stderr)
        sys.exit(1)

    # Create output directory
    output_base = Path(args.output)
    family_dir = output_base / args.family
    family_dir.mkdir(parents=True, exist_ok=True)

    print(f"Converting font family: {args.family}")
    print(f"Output directory: {family_dir}")
    print(f"Font size: {args.size}pt")
    if args.is_2bit:
        print("Mode: 2-bit grayscale")
    print()

    # Define font styles to convert
    styles = [
        ("regular", args.regular),
        ("bold", args.bold),
        ("italic", args.italic),
        ("bold_italic", args.bold_italic),
    ]

    sizes = [14, 16, 18] if args.all_sizes else [args.size]
    success_count = 0
    total_count = 0

    for size in sizes:
        if args.all_sizes:
            size_suffix = f"_{size}"
            print(f"Size: {size}pt")
        else:
            size_suffix = ""

        for style_name, font_path in styles:
            if font_path is None:
                continue

            total_count += 1
            output_file = family_dir / f"{style_name}{size_suffix}.epdfont"

            if convert_font(
                Path(font_path),
                output_file,
                f"{args.family}_{style_name}",
                size,
                args.is_2bit,
            ):
                success_count += 1

    print()
    print(f"Converted {success_count}/{total_count} fonts")

    if success_count > 0:
        print()
        print("To use this font in your theme, add to your .theme file:")
        print()
        print("[fonts]")
        print(f"reader_font = {args.family}")
        print(f"ui_font = {args.family}")
        print()
        print(f"Then copy the '{args.family}' folder to /fonts/ on your SD card.")

    return 0 if success_count == total_count else 1


if __name__ == "__main__":
    sys.exit(main())
