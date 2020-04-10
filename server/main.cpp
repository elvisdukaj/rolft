#include <MessageHeader.h>
#include <fmt/format.h>
#include <boost/asio.hpp>
#include <queue>
using namespace std;

namespace net = boost::asio;

using ChunkHeaderQueue = std::queue<ChunkHeader>;
using ChunkBufferQueue = std::queue<ChunkBuffer>;

class FileTransferSession : std::enable_shared_from_this<FileTransferSession> {
 public:
  FileTransferSession(net::ip::tcp::socket socket) : socket_{std::move(socket)} { readMessageHeader(); }

 private:
  void readMessageHeader() {
    auto self = shared_from_this();

    net::async_read(socket_, net::buffer(messageHeader_.data(), messageHeader_.length()),
                    [this, self](boost::system::error_code, size_t) {
                      fmt::print("Read message header: filename: {}, size: {}\n", messageHeader_.fileName,
                                 messageHeader_.fileLength);
                      readChunkHeader();
                    });
  }

  void readChunkHeader() {
    auto self = shared_from_this();

    chunkHeaderQueue_.emplace();
    auto& lastChunkHeader = chunkHeaderQueue_.front();

    net::async_read(socket_, net::buffer(lastChunkHeader.data(), lastChunkHeader.length()),
                    [this, self](boost::system::error_code, std::size_t) {
                      auto header = chunkHeaderQueue_.front();
                      chunkHeaderQueue_.pop();

                      fmt::print("Read chunk header: index: {}, offset: {}, size: {}\n");

                      if (header.size != 0) {
                        readChunkBody();
                      }
                    });
  }

  void readChunkBody() {
    auto self = shared_from_this();
    chunkBufferQueue_.emplace();
    auto& lastChunkHeader = chunkBufferQueue_.front();
    net::async_read(socket_, net::buffer(lastChunkHeader),
                    [this, self](boost::system::error_code, std::size_t transferedBytes) {
                      fmt::print("Read chunk body [{}]\n", transferedBytes);
                      readChunkHeader();
                    });
  }

 private:
  net::ip::tcp::socket socket_;
  MessageHeader messageHeader_;
  ChunkHeaderQueue chunkHeaderQueue_;
  ChunkBufferQueue chunkBufferQueue_;
};

class Server {
 public:
  Server(net::io_context& ioContext, net::ip::tcp::endpoint endpont)
      : ioContext_{ioContext}, acceptor_{ioContext_, endpont} {}

 private:
  void accept() {
    acceptor_.async_accept([this](boost::system::error_code ec, net::ip::tcp::socket socket) {
      if (!ec) {
        fmt::print("New file transfer session: endpoint!\n");
        std::make_shared<FileTransferSession>(std::move(socket));
      }
      accept();
    });
    ;
  }

 private:
  net::io_context& ioContext_;
  net::ip::tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
  try {
    if (argc < 2) {
      std::printf("Usage: chat_server <port>\n");
      return 1;
    }

    boost::asio::io_context io_context;

    net::ip::tcp::endpoint endpoint(net::ip::tcp::v4(), std::atoi(argv[1]));
    Server server(io_context, endpoint);

    io_context.run();
  } catch (std::exception& e) {
    fmt::print("Exception: {}\n", e.what());
  }

  return 0;
}
