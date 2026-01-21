# Architecture

This document describes the internal architecture and subsystems of Papyrix.

## Font System

### Pipeline

```
Storage → EpdFontLoader → FontManager → GfxRenderer → Display
```

1. **Storage**: Fonts loaded from flash (builtin) or SD card (custom)
2. **EpdFontLoader**: Parses `.epdfont` binary format, provides glyph lookup
3. **FontManager**: Manages font lifecycle, handles loading/unloading
4. **GfxRenderer**: Renders text using font glyphs
5. **Display**: Final output to e-paper

### Memory

- **Builtin fonts**: Flash (DROM), ~20 bytes RAM per wrapper
- **Custom fonts**: SD card → heap, 300-500 KB each

### `.epdfont` Format

Binary format with sections:

```
Header → Metrics → Unicode Intervals → Glyphs → Bitmap
```

- **Header**: Magic, version, font metadata
- **Metrics**: Line height, ascender, descender
- **Unicode Intervals**: Ranges of supported codepoints
- **Glyphs**: Per-character metrics and bitmap offsets
- **Bitmap**: 1-bit or 2-bit packed glyph data

### Key Files

- `lib/EpdFont/` — Format parsing, loading, glyph lookup
- `src/FontManager.h/cpp` — Font lifecycle management
- `lib/GfxRenderer/` — Text rendering with fonts
- `scripts/convert-fonts.mjs` — TTF/OTF to `.epdfont` conversion

### CJK Support

CJK fonts use binary search for glyph lookup: O(log n) complexity. Text can break at any character boundary (no word-based line breaking).

## CSS Parser

### Pipeline

```
EPUB Load → ContentOpfParser → CssParser → ChapterHtmlSlimParser → Page
```

1. **ContentOpfParser**: Discovers CSS files in EPUB manifest (media-type contains "css")
2. **CssParser**: Parses CSS files, builds style map keyed by selector
3. **ChapterHtmlSlimParser**: Queries CSS for each element, applies styles during page layout

### Supported Properties

- **text-align** (left, right, center, justify) — Block alignment
- **font-style** (normal, italic) — Italic text
- **font-weight** (normal, bold, 700+) — Bold text
- **text-indent** (px, em) — First-line indent
- **margin-top/bottom** (em, %) — Extra line spacing

### Supported Selectors

- **Tag selectors**: `p`, `div`, `span`
- **Class selectors**: `.classname`
- **Tag.class selectors**: `p.classname`
- **Comma-separated**: `h1, h2, h3`
- **Inline styles**: `style="text-align: center"`

### Key Files

- `lib/Epub/Epub/css/CssStyle.h` — Style enums and struct
- `lib/Epub/Epub/css/CssParser.h/cpp` — CSS file parsing
- `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp` — Style application during HTML parsing
