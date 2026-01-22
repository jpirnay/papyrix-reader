#include "CoverHelpers.h"

#include <Bitmap.h>
#include <CrossPointSettings.h>
#include <GfxRenderer.h>
#include <HardwareSerial.h>
#include <JpegToBmpConverter.h>
#include <PngToBmpConverter.h>
#include <SDCardManager.h>

namespace CoverHelpers {

bool renderCoverFromBmp(GfxRenderer& renderer, const std::string& bmpPath, int marginTop, int marginRight,
                        int marginBottom, int marginLeft, int& pagesUntilFullRefresh) {
  FsFile coverFile;
  if (!SdMan.openFileForRead("CVR", bmpPath, coverFile)) {
    Serial.printf("[%lu] [CVR] Failed to open cover BMP: %s\n", millis(), bmpPath.c_str());
    return false;
  }

  Bitmap bitmap(coverFile);
  if (bitmap.parseHeaders() != BmpReaderError::Ok) {
    coverFile.close();
    Serial.printf("[%lu] [CVR] Failed to parse cover BMP headers\n", millis());
    return false;
  }

  // Calculate viewport (accounting for margins)
  const int viewportWidth = renderer.getScreenWidth() - marginLeft - marginRight;
  const int viewportHeight = renderer.getScreenHeight() - marginTop - marginBottom;

  // Center image in viewport
  auto rect = calculateCenteredRect(bitmap.getWidth(), bitmap.getHeight(), marginLeft, marginTop, viewportWidth,
                                    viewportHeight);

  renderer.drawBitmap(bitmap, rect.x, rect.y, rect.width, rect.height);

  // Display with refresh logic
  if (pagesUntilFullRefresh <= 1) {
    renderer.displayBuffer(EInkDisplay::HALF_REFRESH);
    pagesUntilFullRefresh = SETTINGS.getPagesPerRefreshValue();
  } else {
    renderer.displayBuffer();
    pagesUntilFullRefresh--;
  }

  // Grayscale rendering (if bitmap supports it)
  if (bitmap.hasGreyscale()) {
    renderer.storeBwBuffer();

    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
    renderer.drawBitmap(bitmap, rect.x, rect.y, rect.width, rect.height);
    renderer.copyGrayscaleLsbBuffers();

    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    renderer.drawBitmap(bitmap, rect.x, rect.y, rect.width, rect.height);
    renderer.copyGrayscaleMsbBuffers();

    renderer.displayGrayBuffer();
    renderer.setRenderMode(GfxRenderer::BW);
    renderer.restoreBwBuffer();
  }

  coverFile.close();
  Serial.printf("[%lu] [CVR] Rendered cover from BMP\n", millis());
  return true;
}

std::string findCoverImage(const std::string& dirPath, const std::string& baseName) {
  // Extensions to search for, in priority order
  const char* extensions[] = {".jpg", ".jpeg", ".png", ".bmp"};

  // Priority 1: Image matching base filename (e.g., book.jpg for book.txt)
  for (const char* ext : extensions) {
    std::string imagePath = dirPath + "/" + baseName + ext;
    if (SdMan.exists(imagePath.c_str())) {
      Serial.printf("[%lu] [CVR] Found cover image: %s\n", millis(), imagePath.c_str());
      return imagePath;
    }
  }

  // Priority 2: Generic cover image in same directory
  for (const char* ext : extensions) {
    std::string imagePath = dirPath + "/cover" + ext;
    if (SdMan.exists(imagePath.c_str())) {
      Serial.printf("[%lu] [CVR] Found cover image: %s\n", millis(), imagePath.c_str());
      return imagePath;
    }
  }

  return "";
}

bool convertImageToBmp(const std::string& inputPath, const std::string& outputPath, const char* logTag) {
  // Check if it's a BMP file (just copy)
  if (FsHelpers::isBmpFile(inputPath)) {
    FsFile src, dst;
    if (!SdMan.openFileForRead(logTag, inputPath, src)) {
      Serial.printf("[%lu] [%s] Failed to open source BMP\n", millis(), logTag);
      return false;
    }
    if (!SdMan.openFileForWrite(logTag, outputPath, dst)) {
      src.close();
      Serial.printf("[%lu] [%s] Failed to create destination BMP\n", millis(), logTag);
      return false;
    }

    uint8_t buffer[512];
    while (src.available()) {
      size_t bytesRead = src.read(buffer, sizeof(buffer));
      dst.write(buffer, bytesRead);
    }

    src.close();
    dst.close();
    Serial.printf("[%lu] [%s] Copied cover BMP: %s\n", millis(), logTag, outputPath.c_str());
    return true;
  }

  // Check if it's a PNG file
  if (FsHelpers::isPngFile(inputPath)) {
    FsFile pngFile;
    if (!SdMan.openFileForRead(logTag, inputPath, pngFile)) {
      Serial.printf("[%lu] [%s] Failed to open PNG file\n", millis(), logTag);
      return false;
    }

    FsFile bmpFile;
    if (!SdMan.openFileForWrite(logTag, outputPath, bmpFile)) {
      pngFile.close();
      Serial.printf("[%lu] [%s] Failed to create BMP file\n", millis(), logTag);
      return false;
    }

    // Use screen dimensions as max (480x800 for Xteink X4)
    const bool success = PngToBmpConverter::pngFileToBmpStreamWithSize(pngFile, bmpFile, 480, 800);

    pngFile.close();
    bmpFile.close();

    if (success) {
      Serial.printf("[%lu] [%s] Generated cover BMP from PNG: %s\n", millis(), logTag, outputPath.c_str());
    } else {
      Serial.printf("[%lu] [%s] Failed to convert PNG to BMP\n", millis(), logTag);
      SdMan.remove(outputPath.c_str());
    }

    return success;
  }

  // Assume JPG/JPEG for any other extension
  FsFile jpegFile;
  if (!SdMan.openFileForRead(logTag, inputPath, jpegFile)) {
    Serial.printf("[%lu] [%s] Failed to open JPEG file\n", millis(), logTag);
    return false;
  }

  FsFile bmpFile;
  if (!SdMan.openFileForWrite(logTag, outputPath, bmpFile)) {
    jpegFile.close();
    Serial.printf("[%lu] [%s] Failed to create BMP file\n", millis(), logTag);
    return false;
  }

  const bool use1Bit = SETTINGS.coverDithering != 0;
  const bool success = use1Bit ? JpegToBmpConverter::jpegFileTo1BitBmpStream(jpegFile, bmpFile)
                               : JpegToBmpConverter::jpegFileToBmpStream(jpegFile, bmpFile);

  jpegFile.close();
  bmpFile.close();

  if (success) {
    Serial.printf("[%lu] [%s] Generated cover BMP from JPEG: %s\n", millis(), logTag, outputPath.c_str());
  } else {
    Serial.printf("[%lu] [%s] Failed to convert JPEG to BMP\n", millis(), logTag);
    SdMan.remove(outputPath.c_str());
  }

  return success;
}

}  // namespace CoverHelpers
