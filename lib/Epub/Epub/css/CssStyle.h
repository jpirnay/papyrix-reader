#pragma once

/**
 * Text alignment values supported by the reader
 */
enum class TextAlign {
  None,    // Default/inherit
  Left,    // Left alignment
  Right,   // Right alignment
  Center,  // Center alignment
  Justify  // Justified text
};

/**
 * Font style values (italic)
 */
enum class CssFontStyle {
  Normal,  // Default normal style
  Italic   // Italic text
};

/**
 * Font weight values (bold)
 */
enum class CssFontWeight {
  Normal,  // Default normal weight
  Bold     // Bold text (700+)
};

/**
 * CssStyle - Represents supported CSS properties for a selector
 *
 * Supported properties:
 * - text-align: left, right, center, justify
 * - font-style: normal, italic
 * - font-weight: normal, bold (700+)
 * - text-indent: px, em units
 * - margin-top/bottom: em units (as line spacing)
 */
struct CssStyle {
  TextAlign textAlign = TextAlign::None;
  bool hasTextAlign = false;

  CssFontStyle fontStyle = CssFontStyle::Normal;
  bool hasFontStyle = false;

  CssFontWeight fontWeight = CssFontWeight::Normal;
  bool hasFontWeight = false;

  float textIndent = 0.0f;
  bool hasTextIndent = false;

  int marginTop = 0;
  bool hasMarginTop = false;

  int marginBottom = 0;
  bool hasMarginBottom = false;

  // Merge another style into this one (other style takes precedence)
  void merge(const CssStyle& other) {
    if (other.hasTextAlign) {
      textAlign = other.textAlign;
      hasTextAlign = true;
    }
    if (other.hasFontStyle) {
      fontStyle = other.fontStyle;
      hasFontStyle = true;
    }
    if (other.hasFontWeight) {
      fontWeight = other.fontWeight;
      hasFontWeight = true;
    }
    if (other.hasTextIndent) {
      textIndent = other.textIndent;
      hasTextIndent = true;
    }
    if (other.hasMarginTop) {
      marginTop = other.marginTop;
      hasMarginTop = true;
    }
    if (other.hasMarginBottom) {
      marginBottom = other.marginBottom;
      hasMarginBottom = true;
    }
  }

  void reset() {
    textAlign = TextAlign::None;
    hasTextAlign = false;
    fontStyle = CssFontStyle::Normal;
    hasFontStyle = false;
    fontWeight = CssFontWeight::Normal;
    hasFontWeight = false;
    textIndent = 0.0f;
    hasTextIndent = false;
    marginTop = 0;
    hasMarginTop = false;
    marginBottom = 0;
    hasMarginBottom = false;
  }
};
