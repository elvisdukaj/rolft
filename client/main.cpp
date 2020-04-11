#include "MessageHeader.h"

#include <fmt/format.h>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>

#include <cstdlib>
#include <iostream>

namespace net = boost::asio;
using net::ip::tcp;

class Client {
 public:
  Client(net::io_context& io_context, const tcp::resolver::results_type& endpoints, boost::filesystem::path file,
         int64_t chunkSize = 1024 * 1024)
      : CHUNK_SIZE{chunkSize}, io_context_(io_context), socket_(io_context_), file_{std::move(file)} {
    net::async_connect(socket_, endpoints, [this](const boost::system::error_code& ec, tcp::endpoint) {
      if (ec) {
        fmt::print("An error occured during message connection: {}\n", ec.message());
        return;
      }

      fmt::print("Client connected esablished\n");
      sendMessageHeader();
      //      sendChunks();
    });
  }

 private:
  void sendMessageHeader() {
    // Prepare message
    messageHeader_.fileLength = boost::filesystem::file_size(file_);
    auto filename = file_.filename().string();
    std::copy(std::begin(filename), std::end(filename), messageHeader_.fileName);

    net::async_write(socket_, net::buffer(messageHeader_.data(), messageHeader_.length()),
                     [this](const boost::system::error_code& ec, size_t transferedBytes) {
                       if (ec) {
                         fmt::print("An error occured during message header transfer: {}\n", ec.message());
                         return;
                       }

                       fmt::print("Header: {} - message header. filename: {}, file size: {}\n", transferedBytes,
                                  std::string{messageHeader_.fileName}, messageHeader_.fileLength);
                       sendChunks();
                     });
  }

  void sendChunks() {
    std::ifstream file(file_.string(), std::fstream::binary);

    int64_t index = 0;

    while (file) {
      // prepare the chunk header
      ChunkHeader chunkHeader;
      chunkHeader.index = index++;
      chunkHeader.offset = file.tellg();

      // buffer to store the chunk
      ChunkBuffer chunk;
      chunk.resize(CHUNK_SIZE);

      // read the chunk from the file
      file.read(chunk.data(), CHUNK_SIZE);
      chunkHeader.size = file.gcount();
      chunk.resize(chunkHeader.size);

      // send the header
      net::async_write(
          socket_, net::buffer(chunkHeader.data(), chunkHeader.length()),
          [this, chunkHeader, chunk = std::move(chunk)](const boost::system::error_code& ec, size_t transferedBytes) {
            if (ec) {
              fmt::format("Error durin trasmission: {}\n", ec.message());
              return;
            }

            fmt::print("Sent {} bytes of chunk index: {}, offset: {}, size: {}\n", transferedBytes, chunkHeader.index,
                       chunkHeader.offset, chunkHeader.size);

            fmt::print("Sending chunk body ({})\n", chunk.size());

            net::async_write(socket_, net::buffer(chunk),
                             [](const boost::system::error_code& ec, size_t transferedBytes) {
                               if (ec) {
                                 fmt::format("Error durin trasmission: {}\n", ec.message());
                                 return;
                               }

                               fmt::print("Sent chunk body: {} bytes\n", transferedBytes);
                             });
          });
    }
    // send the header
    net::async_write(socket_, net::buffer(EOF_CHUNK.data(), EOF_CHUNK.length()),
                     [](const boost::system::error_code& ec, size_t) {
                       if (ec) {
                         fmt::format("An error occured during eof chunk: {}\n", ec.message());
                         return;
                       }
                       fmt::print("EOF chunk\n");
                     });
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
      fmt::print("Usage: client <host> <port> <filename>\n");
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
