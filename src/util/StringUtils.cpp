#include "StringUtils.h"

#include <Utf8.h>

namespace StringUtils {

std::string sanitizeFilename(const std::string& name, size_t maxLength) {
  std::string result;
  result.reserve(name.size());

  for (char c : name) {
    // Replace invalid filename characters with underscore
    if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
      result += '_';
    } else if (c >= 32 && c < 127) {
      // Keep printable ASCII characters
      result += c;
    }
    // Skip non-printable characters
  }

  // Trim leading/trailing spaces and dots
  size_t start = result.find_first_not_of(" .");
  if (start == std::string::npos) {
    return "book";  // Fallback if name is all invalid characters
  }
  size_t end = result.find_last_not_of(" .");
  result = result.substr(start, end - start + 1);

  // Limit filename length
  if (result.length() > maxLength) {
    result.resize(maxLength);
  }

  return result.empty() ? "book" : result;
}

size_t utf8RemoveLastChar(std::string& str) { return ::utf8RemoveLastChar(str); }

void utf8TruncateChars(std::string& str, size_t numChars) { ::utf8TruncateChars(str, numChars); }

}  // namespace StringUtils
