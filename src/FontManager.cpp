#include "FontManager.h"

#include <EpdFontLoader.h>
#include <SDCardManager.h>
#include <StreamingEpdFont.h>
#include <esp_heap_caps.h>

#include <cstring>

#include "config.h"

FontManager& FontManager::instance() {
  static FontManager instance;
  return instance;
}

FontManager::FontManager() = default;

FontManager::~FontManager() {
  unloadAllFonts();
  unloadExternalFont();
}

void FontManager::init(GfxRenderer& r) { renderer = &r; }

bool FontManager::loadFontFamily(const char* familyName, int fontId) {
  if (!renderer || !familyName || !*familyName) {
    return false;
  }

  // Build base path
  char basePath[64];
  snprintf(basePath, sizeof(basePath), "%s/%s", CONFIG_FONTS_DIR, familyName);

  // Check if directory exists
  if (!SdMan.exists(basePath)) {
    return false;
  }

  LoadedFamily family;
  family.fontId = fontId;

  // Only load regular font to save memory (~150KB savings)
  // Bold/italic/bold_italic will use the same regular font
  char fontPath[80];
  snprintf(fontPath, sizeof(fontPath), "%s/regular.epdfont", basePath);

  // Use streaming mode by default (saves ~50KB RAM per font)
  LoadedFont loaded = _useStreamingFonts ? loadStreamingFont(fontPath) : loadSingleFont(fontPath);

  if (!loaded.font && !loaded.streamingFont) {
    return false;
  }

  family.fonts.push_back(loaded);

  // Create font family with regular font for all styles
  // For streaming fonts, we wrap the StreamingEpdFont in an EpdFont adapter
  EpdFont* fontPtr = loaded.font;
  if (loaded.streamingFont) {
    // StreamingEpdFont provides same getData() interface, but we need an EpdFont wrapper
    fontPtr = new EpdFont(loaded.streamingFont->getData());
    // Update the vector entry with the new font pointer (needed for cleanup)
    family.fonts.back().font = fontPtr;
    // Register streaming font with renderer for bitmap access
    renderer->setStreamingFont(fontId, loaded.streamingFont);
  }
  EpdFontFamily fontFamily(fontPtr, fontPtr, fontPtr, fontPtr);
  renderer->insertFont(fontId, fontFamily);

  // Store for cleanup
  loadedFamilies[fontId] = std::move(family);
  return true;
}

FontManager::LoadedFont FontManager::loadSingleFont(const char* path) {
  LoadedFont result = {};

  if (!SdMan.exists(path)) {
    return result;
  }

  EpdFontLoader::LoadResult loaded = EpdFontLoader::loadFromFile(path);
  if (!loaded.success) {
    return result;
  }

  result.data = loaded.fontData;
  result.bitmap = loaded.bitmap;
  result.glyphs = loaded.glyphs;
  result.intervals = loaded.intervals;
  result.font = new EpdFont(result.data);

  // Store sizes for memory profiling
  result.bitmapSize = loaded.bitmapSize;
  result.glyphsSize = loaded.glyphsSize;
  result.intervalsSize = loaded.intervalsSize;

  return result;
}

FontManager::LoadedFont FontManager::loadStreamingFont(const char* path) {
  LoadedFont result = {};

  if (!SdMan.exists(path)) {
    return result;
  }

  StreamingEpdFont* streamingFont = new (std::nothrow) StreamingEpdFont();
  if (!streamingFont) {
    return result;
  }

  if (!streamingFont->load(path)) {
    delete streamingFont;
    return result;
  }

  result.streamingFont = streamingFont;
  // glyphsSize and intervalsSize are tracked inside StreamingEpdFont
  // totalSize() will use streamingFont->getMemoryUsage()

  return result;
}

void FontManager::freeFont(LoadedFont& font) {
  // Free streaming font if present
  if (font.streamingFont) {
    delete font.streamingFont;
    font.streamingFont = nullptr;
  }

  // Free full-load font resources if present
  delete font.font;
  delete font.data;
  delete[] font.bitmap;
  delete[] font.glyphs;
  delete[] font.intervals;

  font = {};
}

void FontManager::unloadFontFamily(int fontId) {
  auto it = loadedFamilies.find(fontId);
  if (it != loadedFamilies.end()) {
    if (renderer) {
      renderer->removeFont(fontId);
    }
    for (auto& f : it->second.fonts) {
      freeFont(f);
    }
    loadedFamilies.erase(it);
  }
}

void FontManager::unloadAllFonts() {
  for (auto& pair : loadedFamilies) {
    if (renderer) {
      renderer->removeFont(pair.second.fontId);
    }
    for (auto& f : pair.second.fonts) {
      freeFont(f);
    }
  }
  loadedFamilies.clear();
}

std::vector<std::string> FontManager::listAvailableFonts() {
  std::vector<std::string> fonts;

  FsFile dir = SdMan.open(CONFIG_FONTS_DIR);
  if (!dir || !dir.isDirectory()) {
    return fonts;
  }

  FsFile entry;
  while (entry.openNext(&dir, O_RDONLY)) {
    if (entry.isDirectory()) {
      char name[64];
      entry.getName(name, sizeof(name));
      // Skip hidden directories
      if (name[0] != '.') {
        // Check if it has at least regular.epdfont
        char regularPath[80];
        snprintf(regularPath, sizeof(regularPath), "%s/%s/regular.epdfont", CONFIG_FONTS_DIR, name);
        if (SdMan.exists(regularPath)) {
          fonts.push_back(name);
        }
      }
    }
    entry.close();
  }
  dir.close();

  return fonts;
}

bool FontManager::fontFamilyExists(const char* familyName) {
  if (!familyName || !*familyName) return false;

  char path[80];
  snprintf(path, sizeof(path), "%s/%s/regular.epdfont", CONFIG_FONTS_DIR, familyName);
  return SdMan.exists(path);
}

int FontManager::getFontId(const char* familyName, int builtinFontId) {
  if (!familyName || !*familyName) {
    return builtinFontId;
  }

  int targetId = generateFontId(familyName);
  if (loadedFamilies.find(targetId) != loadedFamilies.end()) {
    return targetId;
  }

  // Load from SD card
  if (loadFontFamily(familyName, targetId)) {
    return targetId;
  }

  return builtinFontId;
}

int FontManager::generateFontId(const char* familyName) {
  // Simple hash for consistent font IDs
  uint32_t hash = 5381;
  while (*familyName) {
    hash = ((hash << 5) + hash) + static_cast<uint8_t>(*familyName);
    familyName++;
  }
  return static_cast<int>(hash);
}

bool FontManager::isBinFont(const char* familyName) {
  if (!familyName) return false;
  size_t len = strlen(familyName);
  return len > 4 && strcmp(familyName + len - 4, ".bin") == 0;
}

int FontManager::getReaderFontId(const char* familyName, int builtinFontId) {
  if (!familyName || !*familyName) {
    // Using built-in font - unload any custom reader font and external font
    if (_activeReaderFontId != 0 && _activeReaderFontId != builtinFontId) {
      unloadFontFamily(_activeReaderFontId);
      _activeReaderFontId = 0;
    }
    unloadExternalFont();
    return builtinFontId;
  }

  // Handle .bin fonts as external fonts (CJK fallback)
  if (isBinFont(familyName)) {
    // Unload any previous custom .epdfont reader font
    if (_activeReaderFontId != 0 && _activeReaderFontId != builtinFontId) {
      unloadFontFamily(_activeReaderFontId);
      _activeReaderFontId = 0;
    }

    // Load as external font - provides fallback for CJK characters
    loadExternalFont(familyName);
    // Return builtin font ID - ASCII uses built-in, CJK falls back to external
    return builtinFontId;
  }

  int targetId = generateFontId(familyName);

  // If switching to a different custom font, unload previous
  if (_activeReaderFontId != 0 && _activeReaderFontId != targetId) {
    unloadFontFamily(_activeReaderFontId);
  }

  // Load new font if needed
  if (loadedFamilies.find(targetId) == loadedFamilies.end()) {
    if (!loadFontFamily(familyName, targetId)) {
      _activeReaderFontId = 0;
      return builtinFontId;
    }
  }

  _activeReaderFontId = targetId;
  return targetId;
}

bool FontManager::loadExternalFont(const char* filename) {
  if (!renderer || !filename || !*filename) {
    return false;
  }

  char path[80];
  snprintf(path, sizeof(path), "%s/%s", CONFIG_FONTS_DIR, filename);

  // Allocate if needed
  if (!_externalFont) {
    _externalFont = new ExternalFont();
  }

  if (!_externalFont->load(path)) {
    delete _externalFont;
    _externalFont = nullptr;
    return false;
  }

  renderer->setExternalFont(_externalFont);
  return true;
}

void FontManager::unloadExternalFont() {
  if (_externalFont) {
    if (renderer) {
      renderer->setExternalFont(nullptr);
    }
    delete _externalFont;
    _externalFont = nullptr;
  }
}

void FontManager::logFontInfo() const {
  // No-op: debug logging removed
}

void FontManager::logMemoryStatus(const char*) const {
  // No-op: debug logging removed
}

void FontManager::unloadReaderFonts() {
  // Unload any custom .epdfont reader font
  if (_activeReaderFontId != 0) {
    unloadFontFamily(_activeReaderFontId);
    _activeReaderFontId = 0;
  }

  // Unload external CJK font
  unloadExternalFont();
}

size_t FontManager::getCustomFontMemoryUsage() const {
  size_t total = 0;
  for (const auto& pair : loadedFamilies) {
    for (const auto& font : pair.second.fonts) {
      total += font.totalSize();
    }
  }
  return total;
}

size_t FontManager::getExternalFontMemoryUsage() const {
  if (_externalFont && _externalFont->isLoaded()) {
    return ExternalFont::getCacheMemorySize();
  }
  return 0;
}

size_t FontManager::getTotalFontMemoryUsage() const {
  return getCustomFontMemoryUsage() + getExternalFontMemoryUsage();
}

void FontManager::logMemoryReport() const {
  // No-op: debug logging removed
}
