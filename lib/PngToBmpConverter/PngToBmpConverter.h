#pragma once

class FsFile;
class Print;

class PngToBmpConverter {
 public:
  static bool pngFileToBmpStreamWithSize(FsFile& pngFile, Print& bmpOut, int targetMaxWidth, int targetMaxHeight);
};
