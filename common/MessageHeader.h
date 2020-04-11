#ifndef MESSAGEHEADER_H
#define MESSAGEHEADER_H

#include <cstddef>
#include <cstdint>
#include <vector>

template <typename Derivate>
struct Serialize {
  const char* data() const { return reinterpret_cast<const char*>(this); }
  char* data() { return reinterpret_cast<char*>(this); }
  constexpr std::size_t length() const { return sizeof(Derivate); }
};

struct MessageHeader : public Serialize<MessageHeader> {
  int64_t fileLength = 0;
  char fileName[1024] = {'\0'};
};

static_assert(sizeof(MessageHeader) == 1032, "Message header has wrong size!");
static_assert(Serialize<MessageHeader>{}.length() == 1032, "Message header has wrong size!");

struct ChunkHeader : public Serialize<ChunkHeader> {
  ChunkHeader(int64_t index = 1, int64_t offset = 0, int64_t size = 0) : index{index}, offset{offset}, size{size} {}
  int64_t index;
  int64_t offset;
  int64_t size;
};

static_assert(sizeof(ChunkHeader) == 24, "Message header has wrong size!");
static_assert(Serialize<ChunkHeader>{}.length() == 24, "Message chunk has wrong size!");

const ChunkHeader EOF_CHUNK{};

using ChunkBuffer = std::vector<char>;

#endif // MESSAGEHEADER_H
