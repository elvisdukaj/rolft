#ifndef MESSAGEHEADER_H
#define MESSAGEHEADER_H

#include <boost/asio/buffer.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace net = boost::asio;

using ChunkBuffer = std::vector<char>;

struct MessageHeader
{
    MessageHeader(fs::path path)
        : path{std::move(path)}, size{static_cast<int64_t>(fs::file_size(this->path))}
    {}

    fs::path path;
    int64_t size;
};

struct ChunkHeader
{
    int64_t index;
    int64_t offset;
    ino64_t size;
};

struct Chunk
{
    ChunkHeader header;
    ChunkBuffer body;
};

class IProtocol
{
public:
    virtual ~IProtocol() {}
    virtual net::const_buffer makeMessageHeader(fs::path path) = 0;
    virtual std::array<net::const_buffer, 2> makeChunk(std::ifstream &file) = 0;

    virtual MessageHeader parseMessageHeader(const char *data) = 0;
    virtual Chunk parseChunk(const char *data) = 0;
};

class Protocol : public IProtocol
{
public:
    net::const_buffer makeMessageHeader(fs::path path) override;
    std::array<net::const_buffer, 2> makeChunk(std::ifstream &file) override;

    MessageHeader parseMessageHeader(const char *data) override;
    Chunk parseChunk(const char *data) override;
};

class ICommunication
{};

#endif // MESSAGEHEADER_H
