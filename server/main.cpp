////
//// chat_server.cpp
//// ~~~~~~~~~~~~~~~
////
//// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
////
//// Distributed under the Boost Software License, Version 1.0. (See accompanying
//// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
////

//#include <boost/asio.hpp>
//#include <cstdlib>
//#include <deque>
//#include <iostream>
//#include <list>
//#include <memory>
//#include <set>
//#include <utility>
//#include "chat_message.hpp"

// using boost::asio::ip::tcp;

////----------------------------------------------------------------------

// typedef std::deque<chat_message> chat_message_queue;

////----------------------------------------------------------------------

// class chat_participant {
// public:
//  virtual ~chat_participant() {}
//  virtual void deliver(const chat_message& msg) = 0;
//};

// typedef std::shared_ptr<chat_participant> chat_participant_ptr;

////----------------------------------------------------------------------

// class chat_room {
// public:
//  void join(chat_participant_ptr participant) {
//    participants_.insert(participant);
//    for (auto msg : recent_msgs_) participant->deliver(msg);
//  }

//  void leave(chat_participant_ptr participant) { participants_.erase(participant); }

//  void deliver(const chat_message& msg) {
//    recent_msgs_.push_back(msg);
//    while (recent_msgs_.size() > max_recent_msgs) recent_msgs_.pop_front();

//    for (auto participant : participants_) participant->deliver(msg);
//  }

// private:
//  std::set<chat_participant_ptr> participants_;
//  enum { max_recent_msgs = 100 };
//  chat_message_queue recent_msgs_;
//};

////----------------------------------------------------------------------

// class chat_session : public chat_participant, public std::enable_shared_from_this<chat_session> {
// public:
//  chat_session(tcp::socket socket, chat_room& room) : socket_(std::move(socket)), room_(room) {}

//  void start() {
//    room_.join(shared_from_this());
//    do_read_header();
//  }

//  void deliver(const chat_message& msg) {
//    bool write_in_progress = !write_msgs_.empty();
//    write_msgs_.push_back(msg);
//    if (!write_in_progress) {
//      do_write();
//    }
//  }

// private:
//  void do_read_header() {
//    auto self(shared_from_this());
//    boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.data(), chat_message::header_length),
//                            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
//                              if (!ec && read_msg_.decode_header()) {
//                                do_read_body();
//                              } else {
//                                room_.leave(shared_from_this());
//                              }
//                            });
//  }

//  void do_read_body() {
//    auto self(shared_from_this());
//    boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
//                            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
//                              if (!ec) {
//                                room_.deliver(read_msg_);
//                                do_read_header();
//                              } else {
//                                room_.leave(shared_from_this());
//                              }
//                            });
//  }

//  void do_write() {
//    auto self(shared_from_this());
//    boost::asio::async_write(socket_, boost::asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
//                             [this, self](boost::system::error_code ec, std::size_t /*length*/) {
//                               if (!ec) {
//                                 write_msgs_.pop_front();
//                                 if (!write_msgs_.empty()) {
//                                   do_write();
//                                 }
//                               } else {
//                                 room_.leave(shared_from_this());
//                               }
//                             });
//  }

//  tcp::socket socket_;
//  chat_room& room_;
//  chat_message read_msg_;
//  chat_message_queue write_msgs_;
//};

////----------------------------------------------------------------------

// class chat_server {
// public:
//  chat_server(boost::asio::io_context& io_context, const tcp::endpoint& endpoint) : acceptor_(io_context, endpoint) {
//    do_accept();
//  }

// private:
//  void do_accept() {
//    acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
//      if (!ec) {
//        std::make_shared<chat_session>(std::move(socket), room_)->start();
//      }

//      do_accept();
//    });
//  }

//  tcp::acceptor acceptor_;
//  chat_room room_;
//};

////----------------------------------------------------------------------

// int main(int argc, char* argv[]) {
//  try {
//    if (argc < 2) {
//      std::cerr << "Usage: chat_server <port> [<port> ...]\n";
//      return 1;
//    }

//    boost::asio::io_context io_context;

//    std::list<chat_server> servers;
//    for (int i = 1; i < argc; ++i) {
//      tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[i]));
//      servers.emplace_back(io_context, endpoint);
//    }

//    io_context.run();
//  } catch (std::exception& e) {
//    std::cerr << "Exception: " << e.what() << "\n";
//  }

//  return 0;
//}

#include <MessageHeader.h>
#include <boost/asio.hpp>
#include <iostream>
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
                    [this, self](boost::system::error_code, size_t) { readChunkHeader(); });
  }

  void readChunkHeader() {
    auto self = shared_from_this();

    chunkHeaderQueue_.emplace();
    auto& lastChunkHeader = chunkHeaderQueue_.front();

    net::async_read(socket_, net::buffer(lastChunkHeader.data(), lastChunkHeader.length()),
                    [this, self](boost::system::error_code, std::size_t) {
                      auto header = chunkHeaderQueue_.front();
                      chunkHeaderQueue_.pop();

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
                    [this, self](boost::system::error_code, std::size_t) { readChunkHeader(); });
  }

 private:
  net::ip::tcp::socket socket_;
  MessageHeader messageHeader_;
  ChunkHeaderQueue chunkHeaderQueue_;
  ChunkBufferQueue chunkBufferQueue_;
};

class Server {
 public:
  Server(net::io_context& ioContext) : ioContext_{ioContext} {}

 private:
  net::io_context& ioContext_;
};

int main(int argc, char* argv[]) {
  try {
    if (argc < 2) {
      std::cerr << "Usage: chat_server <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    Server server(io_context);
    net::ip::tcp::endpoint endpoint(net::ip::tcp::v4(), std::atoi(argv[1]));

    io_context.run();
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
