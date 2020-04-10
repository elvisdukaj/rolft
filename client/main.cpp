#include "MessageHeader.h"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>

#include <cstdlib>
#include <iostream>

namespace net = boost::asio;
using net::ip::tcp;

class Client {
 public:
  Client(net::io_context& io_context, const tcp::resolver::results_type& endpoints, boost::filesystem::path&& file)
      : io_context_(io_context), socket_(io_context_), file_{std::move(file)} {
    net::async_connect(socket_, endpoints, [this](boost::system::error_code, tcp::endpoint) { sendMessageHeader(); });
  }

 private:
  void sendMessageHeader() {
    // Prepare message
    MessageHeader header;
    header.fileLength = boost::filesystem::file_size(file_);
    auto filename = file_.filename().string();
    std::copy(std::begin(filename), std::end(filename), header.data());

    net::async_write(socket_, net::buffer(header.data(), header.length()),
                     [this](boost::system::error_code, size_t) { sendChunkHeader(); });
  }

  void sendChunkHeader() {
    static std::size_t CHUNK_SIZE = 1024 * 1024;  // CHUNKS are 1 MByte

    std::ifstream file(file_.string(), std::fstream::binary);

    int64_t index = 0;

    while (file) {
      // buffer to store the chunk
      ChunkBuffer chunk;
      chunk.resize(CHUNK_SIZE);

      // prepare the chunk header
      ChunkHeader chunkHeader;
      chunkHeader.index = index++;
      chunkHeader.offset = file.tellg();

      // read the chunk from the file
      file.read(chunk.data(), CHUNK_SIZE);
      auto readBytes = file.gcount();

      chunkHeader.size = readBytes;

      // send the header
      net::async_write(socket_, net::buffer(chunkHeader.data(), chunkHeader.length()),
                       [this, chunkBuffer = std::move(chunk)](boost::system::error_code, size_t) mutable {
                         sendChunkBody(std::move(chunkBuffer));
                       });
    }
  }

  void sendChunkBody(ChunkBuffer&& chunk) {
    // write the chink
    net::async_write(socket_, net::buffer(chunk), [](boost::system::error_code, size_t) {});
  }

  void sendEmptyChunk() {
    // send the header
    net::async_write(socket_, net::buffer(EOF_CHUNK.data(), EOF_CHUNK.length()),
                     [](boost::system::error_code, size_t) {});
  }

 private:
  boost::asio::io_context& io_context_;
  tcp::socket socket_;
  boost::filesystem::path file_;
};

int main(int argc, char* argv[]) {
  try {
    if (argc != 4) {
      std::cerr << "Usage: chat_client <host> <port> <filename>\n";
      return 1;
    }

    // used for async operations
    net::io_context io_context;

    // resolver will connect to the server
    tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve(argv[1], argv[2]);

    // create the client and start all async operations
    Client client(io_context, endpoints, {argv[3]});

    // it will block until all async operation will be done!
    io_context.run();

  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
