#include "ImageBlock.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HardwareSerial.h>
#include <SDCardManager.h>
#include <Serialization.h>

void ImageBlock::render(GfxRenderer& renderer, const int x, const int y) const {
  if (cachedBmpPath.empty()) {
    return;
  }

  FsFile bmpFile;
  if (!SdMan.openFileForRead("IMB", cachedBmpPath, bmpFile)) {
    Serial.printf("[%lu] [IMB] Failed to open cached BMP: %s\n", millis(), cachedBmpPath.c_str());
    return;
  }

  Bitmap bitmap(bmpFile, true);
  const BmpReaderError err = bitmap.parseHeaders();
  if (err != BmpReaderError::Ok) {
    Serial.printf("[%lu] [IMB] BMP parse error: %s\n", millis(), Bitmap::errorToString(err));
    bmpFile.close();
    return;
  }

  renderer.drawBitmap(bitmap, x, y, width, height);
  bmpFile.close();
}

bool ImageBlock::serialize(FsFile& file) const {
  serialization::writeString(file, cachedBmpPath);
  serialization::writePod(file, width);
  serialization::writePod(file, height);
  return true;
}

std::unique_ptr<ImageBlock> ImageBlock::deserialize(FsFile& file) {
  std::string path;
  uint16_t w, h;

  serialization::readString(file, path);
  serialization::readPod(file, w);
  serialization::readPod(file, h);

  // Sanity check: prevent unreasonable dimensions from corrupted data
  if (w > 2000 || h > 2000) {
    Serial.printf("[%lu] [IMB] Deserialization failed: dimensions %ux%u exceed maximum\n", millis(), w, h);
    return nullptr;
  }

  return std::unique_ptr<ImageBlock>(new ImageBlock(std::move(path), w, h));
}
