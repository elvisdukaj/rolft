#include <MessageHeader.h>
#include <fmt/format.h>
#include <boost/asio.hpp>
#include <queue>
using namespace std;

namespace net = boost::asio;

using ChunkHeaderQueue = std::queue<ChunkHeader>;
using ChunkBufferQueue = std::queue<ChunkBuffer>;

class FileTransferSession : public std::enable_shared_from_this<FileTransferSession> {
 public:
  FileTransferSession(net::ip::tcp::socket socket) : socket_{std::move(socket)} { readMessageHeader(); }

  //  void start() { readMessageHeader(); }

 private:
  void readMessageHeader() {
    net::async_read(socket_, net::buffer(messageHeader_.data(), messageHeader_.length()),
                    [this](const boost::system::error_code& ec, size_t transferedBytes) {
                      if (ec) {
                        fmt::print("An error occured during message header reading: {}\n", ec.message());
                        return;
                      }

                      fmt::print("Read {} bytes: message header: filename: {}, size: {}\n", transferedBytes,
                                 messageHeader_.fileName, messageHeader_.fileLength);

                      readChunkHeader();
                    });
  }

  void readChunkHeader() {
    // push a new chunk header in the queue
    //    chunkHeaderQueue_.emplace();
    //    auto& lastChunkHeader = chunkHeaderQueue_.front();

    net::async_read(
        socket_, net::buffer(chunkHeader_.data(), chunkHeader_.length()),
        [this, chunkHeader = chunkHeader_](const boost::system::error_code& ec, std::size_t transferedBytes) {
          if (ec) {
            fmt::format("An error occured while reading chunk header: {}\n", ec.message());
            return;
          }

          // get the header from the queue
          //                      auto header = chunkHeaderQueue_.front();

          fmt::print("Read {} bytes chunk header: index: {}, offset: {}, size: {}\n", transferedBytes,
                     chunkHeader.index, chunkHeader.offset, chunkHeader.size);

          if (chunkHeader.size != 0) readChunkBody(chunkHeader);
        });
  }

  void readChunkBody(ChunkHeader header) {
    //    auto self = shared_from_this();

    // create a new chunk and resize it of the header chunk size
    ChunkBuffer buffer;
    buffer.resize(static_cast<ChunkBuffer::size_type>(header.size));

    // push the chunk in the queue
    chunkBufferQueue_.push(std::move(buffer));

    // get the chunk
    auto& lastChunkBuffer = chunkBufferQueue_.front();

    net::async_read(socket_, net::buffer(lastChunkBuffer),
                    [this /*, self*/](const boost::system::error_code& ec, std::size_t transferedBytes) {
                      if (ec) {
                        fmt::print("An error occured while reading the chunk: {}\n", ec.message());
                        return;
                      }

                      fmt::print("Queue size: {}]\n", chunkBufferQueue_.size());

                      fmt::print("Read chunk body [{}]\n", transferedBytes);
                      //                      chunkHeaderQueue_.pop();
                      chunkBufferQueue_.pop();

                      // read a new chunk header
                      readChunkHeader();
                    });
  }

 private:
  net::ip::tcp::socket socket_;
  MessageHeader messageHeader_;
  ChunkHeader chunkHeader_;
  //  ChunkHeaderQueue chunkHeaderQueue_;
  ChunkBufferQueue chunkBufferQueue_;
};

class Server {
 public:
  Server(net::io_context& ioContext, net::ip::tcp::endpoint endpont)
      : ioContext_{ioContext}, acceptor_{ioContext_, endpont} {
    accept();
  }

 private:
  void accept() {
    acceptor_.async_accept([this](boost::system::error_code ec, net::ip::tcp::socket socket) {
      if (!ec) {
        fmt::print("New file transfer session: endpoint!\n");
        //        std::make_shared<FileTransferSession>(std::move(socket))->start();
        fileTransferSessions_.emplace_back(socket);
      }
      accept();
    });
    ;
  }

 private:
  net::io_context& ioContext_;
  net::ip::tcp::acceptor acceptor_;
  std::vector<std::unique_ptr<FileTransferSession>> fileTransferSessions_;
};

int main(int argc, char* argv[]) {
  try {
    if (argc < 2) {
      std::printf("Usage: server <port>\n");
      return 1;
    }

    boost::asio::io_context ioContext;

    net::ip::tcp::endpoint endpoint(net::ip::tcp::v4(), std::atoi(argv[1]));
    Server server(ioContext, endpoint);

    net::signal_set signals(ioContext, SIGINT, SIGTERM);
    signals.async_wait([&ioContext](const boost::system::error_code& ec, int sig) { ioContext.stop(); });

    ioContext.run();

  } catch (std::exception& e) {
    fmt::print("Exception: {}\n", e.what());
  }

  return 0;
}
