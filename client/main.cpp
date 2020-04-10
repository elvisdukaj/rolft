#include "MessageHeader.h"

#include <fmt/format.h>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>

#include <cstdlib>

namespace net = boost::asio;
using net::ip::tcp;

class Client {
 public:
  Client(net::io_context& io_context, const tcp::resolver::results_type& endpoints, boost::filesystem::path file,
         int64_t chunkSize = 1024 * 1024)
      : CHUNK_SIZE{chunkSize}, io_context_(io_context), socket_(io_context_), file_{std::move(file)} {
    memset(&messageHeader_, 0, sizeof(MessageHeader));
    net::async_connect(socket_, endpoints, [this](boost::system::error_code, tcp::endpoint) {
      fmt::print("Client connected with the server\n");
      sendMessageHeader();
    });
  }

 private:
  void sendMessageHeader() {
    // Prepare message
    messageHeader_.fileLength = boost::filesystem::file_size(file_);
    auto filename = file_.filename().string();
    std::copy(std::begin(filename), std::end(filename), messageHeader_.data());

    net::async_write(socket_, net::buffer(messageHeader_.data(), messageHeader_.length()),
                     [this](boost::system::error_code, size_t) {
                       fmt::print("Writing message header. filename: {}, file size: {}",
                                  std::string{messageHeader_.fileName}, messageHeader_.length());
                       sendChunks();
                     });
  }

  void sendChunks() {
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
                       [this, chunkBuffer = std::move(chunk), chunkHeader](boost::system::error_code, size_t) mutable {
                         fmt::print("Sending chunk index: {}, offset: {}, size: {}\n", chunkHeader.index,
                                    chunkHeader.offset, chunkHeader.size);

                         // send the chunk data
                         net::async_write(socket_, net::buffer(chunkBuffer),
                                          [](boost::system::error_code, size_t transferedBytes) {
                                            fmt::print("Sent chunk body: {} bytes\n", transferedBytes);
                                          });
                       });
    }
    // send the header
    net::async_write(socket_, net::buffer(EOF_CHUNK.data(), EOF_CHUNK.length()),
                     [](boost::system::error_code, size_t) {});
  }

 private:
  const int64_t CHUNK_SIZE;
  boost::asio::io_context& io_context_;
  tcp::socket socket_;
  boost::filesystem::path file_;
  MessageHeader messageHeader_;
};

int main(int argc, char* argv[]) {
  try {
    if (argc != 4) {
      fmt::print("Usage: chat_client <host> <port> <filename>\n");
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
    fmt::print("Exception: {}\n", e.what());
  }

  return 0;
}
