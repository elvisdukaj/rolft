#include "MessageHeader.h"

#include <fmt/format.h>
#include <boost/asio.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <queue>

using ChunkHeaderQueue = std::queue<ChunkHeader>;
using ChunkBufferQueue = std::queue<ChunkBuffer>;

namespace net = boost::asio;
namespace fs = std::filesystem;
using net::ip::tcp;

class Client {
 public:
  Client(net::io_context &io_context, const tcp::resolver::results_type &endpoints,
         const fs::path &file, int64_t chunkSize = 1024 * 1024)
      : CHUNK_SIZE{chunkSize},
        io_context_(io_context),
        socket_(io_context_),
        filePath_{file},
        chunkIndex_{0ll},
        totalTransferredBytes_{0ll} {
    net::async_connect(socket_, endpoints, [this](const auto &ec, auto) {
      if (ec) {
        fmt::print(
            "An error occured during message "
            "connection: {}\n",
            ec.message());
        return;
      }

      fmt::print("Client connected esablished\n");
      sendMessageHeader();
    });
  }

  ~Client() { fmt::print("Done"); }

 private:
  void sendMessageHeader() {
    // Prepare message
    messageHeader_.fileLength = fs::file_size(filePath_);
    auto filename = filePath_.filename().string();
    std::copy(std::begin(filename), std::end(filename), messageHeader_.fileName);

    net::async_write(socket_, net::buffer(messageHeader_.data(), messageHeader_.length()),
                     [this](const auto &ec, auto transferedBytes) {
                       if (ec) {
                         fmt::print(
                             "An error occured during message "
                             "header transfer: {}\n",
                             ec.message());
                         return;
                       }

                       fmt::print(
                           "Header: {} - message header. "
                           "filename: {}, file size: {}\n",
                           transferedBytes, std::string{messageHeader_.fileName},
                           messageHeader_.fileLength);

                       file_ = std::ifstream{filePath_, std::ios::binary};
                       sendFile();
                     });
  }

  void sendFile() {
    if (!file_) return;

    // prepare the chunk header
    chunkHeaderQueue_.emplace();
    chunkBufferQueue_.emplace();
    auto &header = chunkHeaderQueue_.front();
    auto &body = chunkBufferQueue_.front();

    header.index = chunkIndex_++;
    header.offset = file_.tellg();

    body.resize(CHUNK_SIZE);

    // read the chunk from the file
    file_.read(body.data(), CHUNK_SIZE);
    header.size = file_.gcount();
    body.resize(header.size);

    std::array<net::const_buffer, 2> chunk = {net::buffer(header.data(), header.length()),
                                              net::buffer(body)};

    net::async_write(socket_, chunk, [this](const auto &ec, auto transferedBytes) {
      if (ec) {
        fmt::print("An error occured during write: {}\n", ec.message());
        return;
      }

      auto &body = chunkBufferQueue_.front();
      auto &header = chunkHeaderQueue_.front();

      fmt::print("Send a chunk {:7} bytes: header: {{{:3} {:7} {:7}}}, body {:7} bytes\n",
                 transferedBytes, header.index, header.offset, header.size, body.size());

      totalTransferredBytes_ += body.size();

      if (totalTransferredBytes_ == messageHeader_.fileLength) {
        fmt::print("Trasmission complete");
        return;
      }

      sendFile();
    });
  }

 private:
  const int64_t CHUNK_SIZE;
  boost::asio::io_context &io_context_;
  tcp::socket socket_;
  std::ifstream file_;
  fs::path filePath_;
  MessageHeader messageHeader_;
  ChunkHeaderQueue chunkHeaderQueue_;
  ChunkBufferQueue chunkBufferQueue_;
  int64_t chunkIndex_;
  int64_t totalTransferredBytes_;
};

int main(int argc, char *argv[]) {
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

    std::cout << std::endl;

  } catch (std::exception &e) {
    fmt::print("Exception: {}\n", e.what());
  }

  return 0;
}
