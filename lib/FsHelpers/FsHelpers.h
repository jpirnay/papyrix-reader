#pragma once
#include <string>

class FsHelpers {
 public:
  static std::string normalisePath(const std::string& path);

  // Check if a filename should be hidden from file browsers
  // Note: Does NOT check for "." prefix - caller should check that separately
  static bool isHiddenFsItem(const char* name);
};
