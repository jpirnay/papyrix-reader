# Customization Guide

This guide explains how to create custom themes and add custom fonts to Papyrix.

---

## Custom Themes

Papyrix supports user-customizable themes stored on the SD card. Themes control colors, layout options, and button mappings.

### Theme File Location

Theme files are stored in the `/config/themes/` directory on the SD card:

```
/config/themes/
├── light.theme      # Default light theme
├── dark.theme       # Default dark theme
└── my-custom.theme  # Your custom theme
```

When you first use the device, default `light.theme` and `dark.theme` files are created automatically.

### Creating a Custom Theme

1. Copy [example.theme](example.theme) or an existing theme file from your device
2. Rename it (e.g., `my-custom.theme`)
3. Edit the file with any text editor
4. Place it in `/config/themes/` on your SD card
5. Restart the device and select your theme in **Settings > Reader > Theme**

### Theme File Format

Theme files use a simple INI format:

```ini
# Papyrix Theme Configuration
# Edit values and restart device to apply

[theme]
name = My Custom Theme    # Display name shown in Settings UI (optional)

[colors]
inverted_mode = false     # true = dark mode, false = light mode
background = white        # white or black

[selection]
fill_color = black        # Selection highlight color
text_color = white        # Text on selection

[text]
primary_color = black     # Normal text color
secondary_color = black   # Secondary/dimmed text color

[layout]
margin_top = 9            # Top margin in pixels
margin_side = 3           # Side margin in pixels
item_height = 30          # Menu item height
item_spacing = 0          # Space between menu items
front_buttons = bclr      # bclr or lrbc (see below)

[fonts]
ui_font =                 # UI font family (empty = builtin)
reader_font_small =       # Reader font for small size (empty = builtin)
reader_font_medium =      # Reader font for medium size (empty = builtin)
reader_font_large =       # Reader font for large size (empty = builtin)
```

### Configuration Options

#### Theme Section

Optional metadata for the theme:

- **name** - Display name shown in the Settings UI
  - If not specified, the filename (without extension) is used
  - Example: `name = Dark Noto Serif`

#### Colors Section

- **inverted_mode** - Enable dark mode (inverted colors)
  - Values: `true` or `false`
- **background** - Screen background color
  - Values: `white` or `black`

#### Selection Section

- **fill_color** - Highlight color for selected items
  - Values: `white` or `black`
- **text_color** - Text color on selected items
  - Values: `white` or `black`

#### Text Section

- **primary_color** - Main text color
  - Values: `white` or `black`
- **secondary_color** - Secondary/dimmed text color
  - Values: `white` or `black`

#### Layout Section

- **margin_top** - Top screen margin in pixels
  - Default: `9`
- **margin_side** - Side screen margins in pixels
  - Default: `3`
- **item_height** - Height of menu items in pixels
  - Default: `30`
  - Minimum: `1` (values of 0 will cause errors)
  - Affects file browser and menu navigation (including long-press page skip)
  - Note: Chapter selection screens use automatic 2-line item heights based on font size
- **item_spacing** - Vertical space between items in pixels
  - Default: `0`
- **front_buttons** - Front button mapping
  - Values: `bclr` (Back, Confirm, Left, Right) or `lrbc` (Left, Right, Back, Confirm)

#### Fonts Section

- **ui_font** - Custom UI font family name
  - Leave empty to use builtin font
- **reader_font_small** - Custom reader font for small size (14pt)
  - Leave empty to use builtin font
- **reader_font_medium** - Custom reader font for medium size (16pt)
  - Leave empty to use builtin font
- **reader_font_large** - Custom reader font for large size (18pt)
  - Leave empty to use builtin font

### Example: Dark Theme

```ini
[colors]
inverted_mode = true
background = black

[selection]
fill_color = white
text_color = black

[text]
primary_color = white
secondary_color = white

[layout]
margin_top = 9
margin_side = 3
item_height = 30
item_spacing = 0
front_buttons = bclr

[fonts]
ui_font =
reader_font_small =
reader_font_medium =
reader_font_large =
```

### Example: Compact Theme

```ini
[colors]
inverted_mode = false
background = white

[selection]
fill_color = black
text_color = white

[text]
primary_color = black
secondary_color = black

[layout]
margin_top = 5
margin_side = 5
item_height = 25
item_spacing = 2
front_buttons = bclr

[fonts]
ui_font =
reader_font_small =
reader_font_medium =
reader_font_large =
```

### Example: Theme with Custom Fonts

```ini
[colors]
inverted_mode = false
background = white

[selection]
fill_color = black
text_color = white

[text]
primary_color = black
secondary_color = black

[layout]
margin_top = 9
margin_side = 3
item_height = 30
item_spacing = 0
front_buttons = bclr

[fonts]
ui_font = noto-sans
reader_font_small = noto-serif-14
reader_font_medium = noto-serif-16
reader_font_large = noto-serif-18
```

This theme uses custom fonts:
- UI: `/config/fonts/noto-sans/`
- Reader (small): `/config/fonts/noto-serif-14/`
- Reader (medium): `/config/fonts/noto-serif-16/`
- Reader (large): `/config/fonts/noto-serif-18/`

If any font directory doesn't exist, the device falls back to the builtin font for that size.

---

## Custom Fonts

Papyrix supports loading custom fonts from the SD card. Fonts must be pre-converted to the `.epdfont` binary format.

### Font File Location

Custom fonts are stored in the `/config/fonts/` directory, organized by font family:

```
/config/fonts/
├── my-font/
│   ├── regular.epdfont
│   ├── bold.epdfont
│   └── italic.epdfont
└── another-font/
    └── regular.epdfont
```

Each font family is a subdirectory containing one or more style variants.

### Converting Fonts

To create `.epdfont` files from TTF/OTF fonts, use the `convert-fonts.mjs` script included in the firmware source code (`scripts/convert-fonts.mjs`).

#### Requirements

- Node.js 18+
- Install dependencies: `cd scripts && npm install`

#### Basic Usage

Convert a complete font family:

```bash
cd scripts
node convert-fonts.mjs my-font \
    -r MyFont-Regular.ttf \
    -b MyFont-Bold.ttf \
    -i MyFont-Italic.ttf
```

Convert only the regular style:

```bash
node convert-fonts.mjs my-font -r MyFont-Regular.ttf
```

#### Options

- **-r, --regular** - Path to regular style font (required)
- **-b, --bold** - Path to bold style font
- **-i, --italic** - Path to italic style font
- **-o, --output** - Output directory (default: current directory)
- **-s, --size** - Font size in points (default: 16)
- **--2bit** - Generate 2-bit grayscale (smoother but larger)
- **--all-sizes** - Generate all reader sizes (14, 16, 18pt)
- **--bin** - Output raw .bin format for CJK/Thai/Vietnamese (streamed from SD card)
- **--var** - Variable font axis value (e.g., `--var wght=700 --var wdth=100`). Can be specified multiple times for different axes
- **--preview** - Generate HTML preview of rendered glyphs for debugging

#### Examples

```bash
# Convert with custom size
node convert-fonts.mjs my-font -r Font.ttf --size 14

# Output directly to SD card
node convert-fonts.mjs my-font -r Font.ttf -o /Volumes/SDCARD/fonts/

# Generate all sizes for reader font
node convert-fonts.mjs my-font -r Font.ttf --all-sizes

# Use 2-bit grayscale for smoother rendering
node convert-fonts.mjs my-font -r Font.ttf --2bit

# Variable font with specific weight (e.g., Roboto, Inter, or other variable fonts)
node convert-fonts.mjs roboto -r Roboto-VariableFont_wdth,wght.ttf --var wght=400
node convert-fonts.mjs roboto-bold -r Roboto-VariableFont_wdth,wght.ttf --var wght=700
node convert-fonts.mjs roboto-condensed -r Roboto-VariableFont_wdth,wght.ttf --var wght=400 --var wdth=75

# Generate HTML preview to verify font rendering
node convert-fonts.mjs my-font -r Font.ttf --preview
```

The script creates a font family directory structure:

```
my-font/
├── regular.epdfont
├── bold.epdfont
└── italic.epdfont
```

Copy the entire folder to `/config/fonts/` on your SD card.

#### Variable Fonts

Variable fonts (like Roboto, Inter, or Noto Sans) contain multiple weights and widths in a single file. Use the `--var` option to select specific axis values:

**Common axes:**
- **wght** (weight): 100 (thin) to 900 (black), with 400 being regular and 700 being bold
- **wdth** (width): 50 (extra-condensed) to 200 (extra-expanded), with 100 being normal

**Example: Creating a font family from Roboto variable font:**

```bash
# Regular weight
node convert-fonts.mjs roboto -r Roboto-VariableFont_wdth,wght.ttf --var wght=400 --all-sizes

# Bold weight
node convert-fonts.mjs roboto-bold -r Roboto-VariableFont_wdth,wght.ttf --var wght=700 --all-sizes

# Light condensed
node convert-fonts.mjs roboto-light-condensed -r Roboto-VariableFont_wdth,wght.ttf --var wght=300 --var wdth=75 --all-sizes
```

**Detecting variable font axes:**

The converter automatically detects and displays available axes when processing a variable font:

```
Converting: Roboto-VariableFont_wdth,wght.ttf -> regular.epdfont
  Variable font axes: wdth(75-100-100), wght(100-400-900)
  Applied variations: wght=700
```

### Recommended Font Sizes

- **Reader font (Small setting):** 14pt
- **Reader font (Normal setting):** 16pt
- **Reader font (Large setting):** 18pt
- **UI font:** 14-16pt

### Using Custom Fonts in Themes

Once you've created your font files, reference them in your theme configuration:

```ini
[fonts]
ui_font = my-font
reader_font_small = my-font-14
reader_font_medium = my-font-16
reader_font_large = my-font-18
```

Each font family name must match a directory name under `/config/fonts/`. You can use the same font for all sizes, or different fonts for each size.

### Supported Characters

By default, the font converter includes:

- Basic Latin (ASCII) - letters, digits, punctuation
- Latin-1 Supplement - Western European accented characters
- Latin Extended-A/B - Eastern European languages
- Latin Extended Additional - Vietnamese characters
- General punctuation - smart quotes, dashes, ellipsis
- Common currency symbols
- Cyrillic characters
- Combining diacritical marks
- Math operators and arrows

Vietnamese fonts work with standard `.epdfont` format since they use Latin script with additional diacritics.

### CJK and Thai Fonts

The ESP32-C3 has limited RAM (~380KB), so CJK and Thai fonts must use the `.bin` format which streams glyphs from SD card:

```bash
# CJK font (Chinese/Japanese/Korean)
node convert-fonts.mjs noto-sans-cjk -r NotoSansSC-Regular.ttf --bin --size 24

# Thai font
node convert-fonts.mjs noto-sans-thai -r NotoSansThai-Regular.ttf --bin --size 16
```

External `.bin` fonts use ~52KB RAM for glyph caching regardless of character set size.

### Fallback Behavior

If a custom font file is missing, corrupted, or exceeds size limits:

- The device automatically falls back to built-in fonts
- Console shows which font failed and why

**Size limits:**
- `.epdfont` files: max 512KB bitmap data
- `.bin` external fonts: max 32MB file size, max 64x64 pixel glyphs

Built-in fonts are always available:
- **Reader** - Reader font (3 sizes)
- **UI** - UI font
- **Small** - Small text

> **Note:** Custom font loading is optional. The device works perfectly with built-in fonts if no custom fonts are configured.

---

## SD Card Structure

Here's the complete SD card structure for customization:

```
/
├── config/
│   ├── calibre.ini
│   ├── themes/
│   │   ├── light.theme
│   │   ├── dark.theme
│   │   └── custom.theme
│   └── fonts/
│       ├── my-reader-font/
│       │   ├── regular.epdfont
│       │   ├── bold.epdfont
│       │   └── italic.epdfont
│       └── my-ui-font/
│           └── regular.epdfont
├── sleep.bmp              # Custom sleep image (optional)
└── sleep/                 # Multiple sleep images (optional)
    ├── image1.bmp
    └── image2.bmp
```

---

## Example Files

The repository includes example theme and font files in [`docs/examples/`](examples/):

**Themes:**
- **`light-noto-serif.theme`** - Light theme with Noto Serif reader fonts (Latin script)
- **`light-thai.theme`** - Light theme with Noto Sans Thai fonts
- **`light-vietnamese.theme`** - Light theme with Noto Serif Vietnamese fonts
- **`light-cjk-external.theme`** - Light theme with CJK external (.bin) fonts

**Fonts:**
- **`fonts/noto-serif-*/`** - Noto Serif at 14pt, 16pt, 18pt (Latin script)
- **`fonts/noto-sans-thai-*/`** - Noto Sans Thai at 14pt, 16pt, 18pt
- **`fonts/noto-serif-vn-*/`** - Noto Serif Vietnamese at 14pt, 16pt, 18pt
- **`fonts/*.bin`** - CJK external fonts (Source Han Sans CN, KingHwaOldSong)

To use a theme:
1. Copy the `.theme` file to `/config/themes/` on your SD card
2. Copy the corresponding font folders to `/config/fonts/` on your SD card
3. Select the theme in **Settings > Reader > Theme**

### Font Attribution

The example fonts use:
- [Noto Serif](https://fonts.google.com/noto/specimen/Noto+Serif) from Google Fonts (SIL OFL)
- [Noto Sans Thai](https://fonts.google.com/noto/specimen/Noto+Sans+Thai) from Google Fonts (SIL OFL)
- [Source Han Sans CN](https://github.com/adobe-fonts/source-han-sans) from Adobe (SIL OFL)
- KingHwaOldSong (traditional Chinese font)

All fonts are licensed under the [SIL Open Font License (OFL)](https://openfontlicense.org/).
