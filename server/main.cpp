#include <MessageHeader.h>
#include <fmt/format.h>
#include <boost/asio.hpp>
#include <fstream>
#include <queue>
using namespace std;

namespace net = boost::asio;

class FileTransferSession : public std::enable_shared_from_this<FileTransferSession> {
 public:
  FileTransferSession(net::ip::tcp::socket socket) : socket_{std::move(socket)} {}

  void start() { readMessageHeader(); }

 private:
  void readMessageHeader() {
    net::async_read(socket_, net::buffer(messageHeader_.data(), messageHeader_.length()),
                    [self = shared_from_this()](const auto &ec, auto transferedBytes) {
                      if (ec) {
                        fmt::print("An error occured during message header reading: {}\n", ec.message());
                        return;
                      }

                      fmt::print("Read {} bytes: message header: filename: {}, size: {}\n", transferedBytes,
                                 self->messageHeader_.fileName, self->messageHeader_.fileLength);

                      std::ofstream file(self->messageHeader_.fileName, std::ios::binary);

                      self->readChunkHeader(std::move(file));
                    });
  }

  void readChunkHeader(std::ofstream &&file) {
    // push a new chunk header in the queue
    auto header = std::make_shared<ChunkHeader>();
    net::async_read(
        socket_, net::buffer(header->data(), header->length()),
        [self = shared_from_this(), header, file = std::move(file)](const auto &ec, auto transferedBytes) mutable {
          if (ec) {
            fmt::format("An error occured while reading chunk header: {}\n", ec.message());
            return;
          }

          fmt::print("Read {} bytes chunk header: index: {}, offset: {}, size: {}\n", transferedBytes, header->index,
                     header->offset, header->size);

          if (header->size != 0) self->readChunkBody(std::move(file), *header);
        });
  }

  void readChunkBody(std::ofstream &&file, ChunkHeader header) {
    // create a new chunk and resize it of the header chunk size
    auto buffer = std::make_shared<ChunkBuffer>();
    buffer->resize(static_cast<ChunkBuffer::size_type>(header.size));

    net::async_read(
        socket_, net::buffer(*buffer),
        [self = shared_from_this(), buffer, file = std::move(file)](const auto &ec, auto transferedBytes) mutable {
          if (ec) {
            fmt::print("An error occured while reading the chunk: {}\n", ec.message());
            return;
          }

          fmt::print("Read chunk body [{}]\n", transferedBytes);

          file.write(buffer->data(), buffer->size());

          // read a new chunk header
          self->readChunkHeader(std::move(file));
        });
  }

 private:
  net::ip::tcp::socket socket_;
  MessageHeader messageHeader_;
};

class Server {
 public:
  Server(net::io_context &ioContext, net::ip::tcp::endpoint endpont)
      : ioContext_{ioContext}, acceptor_{ioContext_, endpont} {
    accept();
  }

 private:
  void accept() {
    acceptor_.async_accept([this](const auto &ec, auto socket) {
      if (!ec) {
        fmt::print("New file transfer session: endpoint!\n");
        make_shared<FileTransferSession>(std::move(socket))->start();
      }

      accept();
    });
  }

 private:
  net::io_context &ioContext_;
  net::ip::tcp::acceptor acceptor_;
};

int main(int argc, char *argv[]) {
  try {
    if (argc < 2) {
      std::printf("Usage: server <port>\n");
      return 1;
    }

    boost::asio::io_context ioContext;

    net::ip::tcp::endpoint endpoint(net::ip::tcp::v4(), std::atoi(argv[1]));
    Server server(ioContext, endpoint);

    net::signal_set signals(ioContext, SIGINT, SIGTERM);
    signals.async_wait([&ioContext](const auto &ec, int sig) { ioContext.stop(); });

    ioContext.run();

  } catch (std::exception &e) {
    fmt::print("Exception: {}\n", e.what());
  }

  return 0;
}
