#ifndef MESSAGEHEADER_H
#define MESSAGEHEADER_H

#include <cstddef>
#include <cstdint>
#include <vector>

struct MessageHeader {
  const char* data() const { return reinterpret_cast<const char*>(this); }
  char* data() { return reinterpret_cast<char*>(this); }
  std::size_t length() const { return sizeof(MessageHeader); }

  int64_t fileLength;
  char fileName[256];
};

struct ChunkHeader {
  const char* data() const { return reinterpret_cast<const char*>(this); }
  char* data() { return reinterpret_cast<char*>(this); }
  std::size_t length() const { return sizeof(MessageHeader); }

  int64_t index;
  int64_t offset;
  int64_t size;
};

const ChunkHeader EOF_CHUNK{-1, 0};

using ChunkBuffer = std::vector<char>;

#endif // MESSAGEHEADER_H
