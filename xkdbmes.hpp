// "Copyright 2021 Kirill Konevets"

/**
 *   \file xkdbmes.hpp
 *   \brief Sync/async client and async server with protobuf messaging
 */

#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/core/noncopyable.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <functional>
#include <google/protobuf/message.h>
#include <string>

#include "../xkdb/common.hpp"
#include "dstream.hpp"
#include <xkdb.pb.h>

using tcp = boost::asio::ip::tcp;
using Message = google::protobuf::Message;

namespace x_company::xkdbmes {

// forward declare
class AsynClient;

using query_handle_t =
    std::function<void(const xkdb::Query &query, xkdb::Response &resp,
                       const connection_info &info)>;
using auth_handle_t = std::function<bool(const xkdb::Auth &auth)>;
using response_handle_t = std::function<void(xkdb::Response &&resp,
                                             std::shared_ptr<AsynClient> self)>;

/////////////////////////////////////////////////////////////////////////////
//                                  Client                                 //
/////////////////////////////////////////////////////////////////////////////

/**
 *  \brief Synchronous client with protobuf messaging
 */
class Client {
public:
  Client(boost::asio::io_context &ioc, std::string const &host, uint16_t port);

  /**
   *  \brief Write query to socket and block till receiving server response
   */
  xkdb::Response exec(const Message &query);

private:
  boost::asio::io_context &ioc_;
  tcp::socket socket_;
  DelimitedStream dstream_{xkdb::Response::CLIENT_ERROR};
};

/////////////////////////////////////////////////////////////////////////////
//                                AsynClient                               //
/////////////////////////////////////////////////////////////////////////////

/**
 *  \brief Async client with protobuf messaging
 */
class AsynClient : public std::enable_shared_from_this<AsynClient>,
                   boost::noncopyable {
public:
  /**
   * \brief Start async client
   * \param response_handle User defined function that handles server responses
   * \return a pointer that is shared among `exec(query)` calls
   */
  [[nodiscard]] static std::shared_ptr<AsynClient>
  start(boost::asio::io_context &ioc, std::string const &host, uint16_t port,
        const xkdb::Auth &auth, response_handle_t response_handle);

  /**
   * \brief Stop async client
   */
  void stop();

  /**
   * \brief Async write query to socket and handle response in `response_handle`
   */
  void exec(const Message &query);

  /**
   *  \brief Checks if client is connected to server
   */
  bool connected() const { return connected_; }

  /**
   *  \brief Checks if client started
   */
  bool started() { return started_; }

private:
  /**
   * Connect to server asynchronously and try to authenticate, auth response is
   * handled by the first `response_handle` call
   * \param response_handle User defined function that handles server responses
   */
  AsynClient(boost::asio::io_context &ioc, std::string const &host,
             uint16_t port, const xkdb::Auth &auth,
             response_handle_t response_handle);

  void read_();

  void write_(const Message &query);

  boost::asio::io_context &ioc_;
  tcp::resolver resolver_;
  tcp::socket socket_;
  bool connected_{false};
  bool started_{false};
  DelimitedStream dstream_{xkdb::Response::CLIENT_ERROR};
  response_handle_t response_handle_;
};

/////////////////////////////////////////////////////////////////////////////
//                                  Server                                 //
/////////////////////////////////////////////////////////////////////////////

/**
 *  \brief Asynchronous server with protobuf messaging
 */
class Server {
public:
  /**
   *  \param auth_handle User defined function that checks user and password
   *  \param query_handle User defined function that handles query and sets
   * response
   */
  Server(boost::asio::io_context &ioc, uint16_t port, auth_handle_t auth_handle,
         query_handle_t query_handle);

private:
  void do_accept();

  tcp::acceptor acceptor_;
  auth_handle_t auth_handle_;
  query_handle_t query_handle_;
};

/**
 * Session of a client, i.e. a connection. When client object is
 * destroyed the client socket is closed, so server receives asio::error::eof
 * and read/write callback recursion is stoped, hence shared_ptr counter
 * becomes zero and session is destroyed too.
 */
class Session : public std::enable_shared_from_this<Session> {
public:
  Session(tcp::socket socket, auth_handle_t auth_handle,
          query_handle_t query_handle);

  // Start reading/writing messages
  void start() { read_(); }

private:
  void read_();
  void write_();

  tcp::socket socket_;
  bool connected_{false};
  connection_info info_;
  DelimitedStream dstream_{xkdb::Response::SERVER_ERROR};
  auth_handle_t auth_handle_;
  query_handle_t query_handle_;
};

} // namespace x_company::xkdbmes
