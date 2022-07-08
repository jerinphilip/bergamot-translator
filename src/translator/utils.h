#pragma once

#include <iostream>

namespace marian::bergamot {

inline std::string readFromStdin() {
  // Read a large input text blob from stdin
  std::ostringstream inputStream;
  inputStream << std::cin.rdbuf();
  std::string input = inputStream.str();
  return input;
}

/**
 * Converts byte offset into utf-8 aware character position.
 * Unicode checking blatantly stolen from https://stackoverflow.com/a/
 */
int offsetToPosition(std::string const &text, std::size_t offset) {
  // if offset is never bigger than text.size(), and we iterate based on
  // offset, *p will always be within [c_str(), c_str() + size())
  if (offset > text.size()) offset = text.size();

  std::size_t pos = 0;
  for (char const *p = text.c_str(); offset > 0; --offset, p++) {
    if ((*p & 0xc0) != 0x80)  // if is not utf-8 continuation character
      ++pos;
  }
  return pos;
}

/**
 * Other way around: converts utf-8 character position into a byte offset.
 */
std::size_t positionToOffset(std::string const &text, int pos) {
  char const *p = text.c_str(), *end = text.c_str() + text.size();
  // Continue for-loop while pos > 0 or while we're in a multibyte utf-8 char
  for (; p != end && (pos > 0 || (*p & 0xc0) == 0x80); p++) {
    if ((*p & 0xc0) != 0x80) --pos;
  }
  return p - text.c_str();
}

}  // namespace marian::bergamot
