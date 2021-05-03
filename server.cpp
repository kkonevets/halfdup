#include "xkdbmes.hpp"
#include <x-company/Log.hpp>

namespace x_company::xkdbmes {

Server::Server(boost::asio::io_context &ioc, uint16_t port,
               auth_handle_t auth_handle, query_handle_t query_handle)
    : acceptor_(ioc, tcp::endpoint(tcp::v4(), port)), auth_handle_(auth_handle),
      query_handle_(query_handle) {
  do_accept();
}

void Server::do_accept() {
  acceptor_.async_accept([this](boost::system::error_code ec,
                                tcp::socket socket) {
    if (!ec) {
      std::make_shared<Session>(std::move(socket), auth_handle_, query_handle_)
          ->start();
    }
    do_accept();
  });
}

Session::Session(tcp::socket socket, auth_handle_t auth_handle,
                 query_handle_t query_handle)
    : socket_(std::move(socket)), auth_handle_(auth_handle),
      query_handle_(query_handle) {
  boost::system::error_code ec;
  auto remote = socket_.remote_endpoint(ec);
  if (!ec) {
    info_.remote_addr = remote.address().to_string();
    info_.remote_port = remote.port();
  }
  auto local = socket_.local_endpoint(ec);
  if (!ec) {
    info_.local_addr = local.address().to_string();
    info_.local_port = local.port();
  }
}

void Session::read_() {
  auto self(shared_from_this());
  boost::asio::async_read_until(
      socket_, dstream_.buf(), DelimitedStream::DELIM,
      [this, self](boost::system::error_code ec, std::size_t length) {
        if (!ec) {
          xkdb::Response resp;

          if (connected_) {
            xkdb::Query query;
            if (dstream_.parse(query, resp, length)) {
              query_handle_(query, resp, info_);
              resp.set_status(xkdb::Response::OK);
            }
          } else {
            xkdb::Auth auth;
            if (dstream_.parse(auth, resp, length)) {
              if (auth_handle_(auth)) {
                resp.set_status(xkdb::Response::OK);
                info_.user = auth.user();
                info_.auth = connected_ = true;
              } else {
                resp.set_status(xkdb::Response::UNAUTHORIZED);
                resp.set_emsg("server: wrong user or password");
              }
            }
          }

          dstream_.serialize(resp, resp);
          write_();
        } else if (ec == boost::asio::error::eof) {
          ; // it's ok, client closed connection
        } else {
          XK_LOGERR << "xkdbmes read error: " << ec.message() << std::endl;
        }
      });
}

void Session::write_() {
  auto self(shared_from_this());
  boost::asio::async_write(
      socket_, dstream_.buf(),
      [this, self](boost::system::error_code ec, std::size_t /*length*/) {
        if (!ec) {
          read_();
        }
      });
}

} // namespace x_company::xkdbmes
