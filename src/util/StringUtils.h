#pragma once

#include <string>

namespace StringUtils {

/**
 * Sanitize a string for use as a filename.
 * Replaces invalid characters with underscores, trims spaces/dots,
 * and limits length to maxLength characters.
 */
std::string sanitizeFilename(const std::string& name, size_t maxLength = 100);

/**
 * UTF-8 safe string truncation - removes one character from the end.
 * Returns the new size after removing one UTF-8 character.
 */
size_t utf8RemoveLastChar(std::string& str);

/**
 * UTF-8 safe truncation - removes N characters from the end.
 */
void utf8TruncateChars(std::string& str, size_t numChars);

}  // namespace StringUtils
