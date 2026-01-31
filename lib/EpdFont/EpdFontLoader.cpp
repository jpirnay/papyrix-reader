#include "EpdFontLoader.h"

#include <LittleFS.h>
#include <SDCardManager.h>

#include <cstring>

static constexpr uint32_t MAX_BITMAP_SIZE = 512 * 1024;  // 512KB

bool EpdFontLoader::validateMetricsAndMemory(const FileMetrics& metrics) {
  if (metrics.intervalCount > 10000 || metrics.glyphCount > 100000 || metrics.bitmapSize > MAX_BITMAP_SIZE) {
    Serial.printf("[FONTLOAD] Font exceeds size limits (bitmap=%u, max=%u). Using default font.\n", metrics.bitmapSize,
                  MAX_BITMAP_SIZE);
    return false;
  }

  size_t requiredMemory = metrics.intervalCount * sizeof(EpdUnicodeInterval) + metrics.glyphCount * sizeof(EpdGlyph) +
                          metrics.bitmapSize + sizeof(EpdFontData);
  size_t availableHeap = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  if (requiredMemory > availableHeap * 0.8) {
    Serial.printf("[FONTLOAD] Insufficient memory: need %zu, available %zu. Using default font.\n", requiredMemory,
                  availableHeap);
    return false;
  }

  return true;
}

EpdFontLoader::LoadResult EpdFontLoader::loadFromFile(const char* path) {
  LoadResult result = {false, nullptr, nullptr, nullptr, nullptr};

  FsFile file = SdMan.open(path, O_RDONLY);
  if (!file) {
    Serial.printf("[FONTLOAD] Cannot open file: %s\n", path);
    return result;
  }

  // Read and validate header
  FileHeader header;
  if (file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header)) {
    Serial.println("[FONTLOAD] Failed to read header");
    file.close();
    return result;
  }

  if (header.magic != MAGIC) {
    Serial.printf("[FONTLOAD] Invalid magic: 0x%08X (expected 0x%08X)\n", header.magic, MAGIC);
    file.close();
    return result;
  }

  if (header.version != VERSION) {
    Serial.printf("[FONTLOAD] Unsupported version: %d (expected %d)\n", header.version, VERSION);
    file.close();
    return result;
  }

  bool is2Bit = (header.flags & 0x01) != 0;

  // Read metrics
  FileMetrics metrics;
  if (file.read(reinterpret_cast<uint8_t*>(&metrics), sizeof(metrics)) != sizeof(metrics)) {
    Serial.println("[FONTLOAD] Failed to read metrics");
    file.close();
    return result;
  }

  Serial.printf("[FONTLOAD] Font: advanceY=%d, ascender=%d, descender=%d, intervals=%u, glyphs=%u, bitmap=%u\n",
                metrics.advanceY, metrics.ascender, metrics.descender, metrics.intervalCount, metrics.glyphCount,
                metrics.bitmapSize);

  if (!validateMetricsAndMemory(metrics)) {
    file.close();
    return result;
  }

  // Allocate memory
  result.intervals = new (std::nothrow) EpdUnicodeInterval[metrics.intervalCount];
  result.glyphs = new (std::nothrow) EpdGlyph[metrics.glyphCount];
  result.bitmap = new (std::nothrow) uint8_t[metrics.bitmapSize];
  result.fontData = new (std::nothrow) EpdFontData;

  if (!result.intervals || !result.glyphs || !result.bitmap || !result.fontData) {
    Serial.println("[FONTLOAD] Memory allocation failed");
    freeLoadResult(result);
    file.close();
    return result;
  }

  // Read intervals
  size_t intervalsSize = metrics.intervalCount * sizeof(EpdUnicodeInterval);
  if (file.read(reinterpret_cast<uint8_t*>(result.intervals), intervalsSize) != intervalsSize) {
    Serial.println("[FONTLOAD] Failed to read intervals");
    freeLoadResult(result);
    file.close();
    return result;
  }

  // Read glyphs (14 bytes each in binary format, field by field)
  for (uint32_t i = 0; i < metrics.glyphCount; i++) {
    uint8_t glyphData[14];
    if (file.read(glyphData, 14) != 14) {
      Serial.printf("[FONTLOAD] Failed to read glyph %u\n", i);
      freeLoadResult(result);
      file.close();
      return result;
    }
    // Parse fields from binary format
    result.glyphs[i].width = glyphData[0];
    result.glyphs[i].height = glyphData[1];
    result.glyphs[i].advanceX = glyphData[2];
    // glyphData[3] is padding
    result.glyphs[i].left = static_cast<int16_t>(glyphData[4] | (glyphData[5] << 8));
    result.glyphs[i].top = static_cast<int16_t>(glyphData[6] | (glyphData[7] << 8));
    result.glyphs[i].dataLength = static_cast<uint16_t>(glyphData[8] | (glyphData[9] << 8));
    result.glyphs[i].dataOffset =
        static_cast<uint32_t>(glyphData[10] | (glyphData[11] << 8) | (glyphData[12] << 16) | (glyphData[13] << 24));
  }

  // Read bitmap
  if (file.read(result.bitmap, metrics.bitmapSize) != metrics.bitmapSize) {
    Serial.println("[FONTLOAD] Failed to read bitmap");
    freeLoadResult(result);
    file.close();
    return result;
  }

  // Populate font data structure
  result.fontData->bitmap = result.bitmap;
  result.fontData->glyph = result.glyphs;
  result.fontData->intervals = result.intervals;
  result.fontData->intervalCount = metrics.intervalCount;
  result.fontData->advanceY = metrics.advanceY;
  result.fontData->ascender = metrics.ascender;
  result.fontData->descender = metrics.descender;
  result.fontData->is2Bit = is2Bit;

  result.success = true;
  file.close();

  Serial.printf("[FONTLOAD] Successfully loaded font from %s\n", path);
  return result;
}

void EpdFontLoader::freeLoadResult(LoadResult& result) {
  delete result.fontData;
  delete[] result.bitmap;
  delete[] result.glyphs;
  delete[] result.intervals;
  result = {false, nullptr, nullptr, nullptr, nullptr};
}

EpdFontLoader::LoadResult EpdFontLoader::loadFromLittleFS(const char* path) {
  LoadResult result = {false, nullptr, nullptr, nullptr, nullptr};

  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.printf("[FONTLOAD] Cannot open LittleFS file: %s\n", path);
    return result;
  }

  // Read and validate header
  FileHeader header;
  if (file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header)) {
    Serial.println("[FONTLOAD] Failed to read header from LittleFS");
    file.close();
    return result;
  }

  if (header.magic != MAGIC) {
    Serial.printf("[FONTLOAD] Invalid magic: 0x%08X (expected 0x%08X)\n", header.magic, MAGIC);
    file.close();
    return result;
  }

  if (header.version != VERSION) {
    Serial.printf("[FONTLOAD] Unsupported version: %d (expected %d)\n", header.version, VERSION);
    file.close();
    return result;
  }

  bool is2Bit = (header.flags & 0x01) != 0;

  // Read metrics
  FileMetrics metrics;
  if (file.read(reinterpret_cast<uint8_t*>(&metrics), sizeof(metrics)) != sizeof(metrics)) {
    Serial.println("[FONTLOAD] Failed to read metrics from LittleFS");
    file.close();
    return result;
  }

  Serial.printf("[FONTLOAD] Font: advanceY=%d, ascender=%d, descender=%d, intervals=%u, glyphs=%u, bitmap=%u\n",
                metrics.advanceY, metrics.ascender, metrics.descender, metrics.intervalCount, metrics.glyphCount,
                metrics.bitmapSize);

  if (!validateMetricsAndMemory(metrics)) {
    file.close();
    return result;
  }

  // Allocate memory
  result.intervals = new (std::nothrow) EpdUnicodeInterval[metrics.intervalCount];
  result.glyphs = new (std::nothrow) EpdGlyph[metrics.glyphCount];
  result.bitmap = new (std::nothrow) uint8_t[metrics.bitmapSize];
  result.fontData = new (std::nothrow) EpdFontData;

  if (!result.intervals || !result.glyphs || !result.bitmap || !result.fontData) {
    Serial.println("[FONTLOAD] Memory allocation failed");
    freeLoadResult(result);
    file.close();
    return result;
  }

  // Read intervals
  size_t intervalsSize = metrics.intervalCount * sizeof(EpdUnicodeInterval);
  if (file.read(reinterpret_cast<uint8_t*>(result.intervals), intervalsSize) != intervalsSize) {
    Serial.println("[FONTLOAD] Failed to read intervals from LittleFS");
    freeLoadResult(result);
    file.close();
    return result;
  }

  // Read glyphs (14 bytes each in binary format, field by field)
  for (uint32_t i = 0; i < metrics.glyphCount; i++) {
    uint8_t glyphData[14];
    if (file.read(glyphData, 14) != 14) {
      Serial.printf("[FONTLOAD] Failed to read glyph %u from LittleFS\n", i);
      freeLoadResult(result);
      file.close();
      return result;
    }
    // Parse fields from binary format
    result.glyphs[i].width = glyphData[0];
    result.glyphs[i].height = glyphData[1];
    result.glyphs[i].advanceX = glyphData[2];
    // glyphData[3] is padding
    result.glyphs[i].left = static_cast<int16_t>(glyphData[4] | (glyphData[5] << 8));
    result.glyphs[i].top = static_cast<int16_t>(glyphData[6] | (glyphData[7] << 8));
    result.glyphs[i].dataLength = static_cast<uint16_t>(glyphData[8] | (glyphData[9] << 8));
    result.glyphs[i].dataOffset =
        static_cast<uint32_t>(glyphData[10] | (glyphData[11] << 8) | (glyphData[12] << 16) | (glyphData[13] << 24));
  }

  // Read bitmap
  if (file.read(result.bitmap, metrics.bitmapSize) != metrics.bitmapSize) {
    Serial.println("[FONTLOAD] Failed to read bitmap from LittleFS");
    freeLoadResult(result);
    file.close();
    return result;
  }

  // Populate font data structure
  result.fontData->bitmap = result.bitmap;
  result.fontData->glyph = result.glyphs;
  result.fontData->intervals = result.intervals;
  result.fontData->intervalCount = metrics.intervalCount;
  result.fontData->advanceY = metrics.advanceY;
  result.fontData->ascender = metrics.ascender;
  result.fontData->descender = metrics.descender;
  result.fontData->is2Bit = is2Bit;

  result.success = true;
  file.close();

  Serial.printf("[FONTLOAD] Successfully loaded font from LittleFS: %s\n", path);
  return result;
}
