#include "xkdbmes.hpp"

namespace x_company::xkdbmes {

/////////////////////////////////////////////////////////////////////////////
//                                  Client                                 //
/////////////////////////////////////////////////////////////////////////////

Client::Client(boost::asio::io_context &ioc, std::string const &host,
               uint16_t port)
    : ioc_(ioc), socket_(ioc_) {
  tcp::resolver resolver(ioc_);
  tcp::resolver::iterator endpoint =
      resolver.resolve(host, std::to_string(port));
  boost::asio::connect(this->socket_, endpoint);
}

xkdb::Response Client::exec(const Message &query) {
  xkdb::Response resp;

  if (!dstream_.serialize(query, resp)) {
    return resp;
  }
  boost::asio::write(socket_, dstream_.buf());
  auto length =
      boost::asio::read_until(socket_, dstream_.buf(), DelimitedStream::DELIM);
  dstream_.parse(resp, resp, length);
  return resp;
}

/////////////////////////////////////////////////////////////////////////////
//                                AsynClient                               //
/////////////////////////////////////////////////////////////////////////////

AsynClient::AsynClient(boost::asio::io_context &ioc, std::string const &host,
                       uint16_t port, const xkdb::Auth &auth,
                       response_handle_t response_handle)
    : ioc_(ioc), resolver_(ioc), socket_(ioc_), started_(true),
      response_handle_(response_handle) {

  resolver_.async_resolve( //
      host, std::to_string(port),
      [this, &auth](const boost::system::error_code &ec,
                    tcp::resolver::results_type results) {
        if (!ec) {
          boost::asio::async_connect(
              this->socket_, results,
              [this, &auth](const boost::system::error_code &ec,
                            const tcp::endpoint &) {
                if (!ec) {
                  exec(auth);
                } else {
                  stop();
                  boost::asio::detail::throw_error(ec);
                }
              });
        } else {
          stop();
          boost::asio::detail::throw_error(ec);
        }
      });
}

std::shared_ptr<AsynClient>
AsynClient::start(boost::asio::io_context &ioc, std::string const &host,
                  uint16_t port, const xkdb::Auth &auth,
                  response_handle_t response_handle) {
  return std::shared_ptr<AsynClient>(
      new AsynClient(ioc, host, port, auth, response_handle));
}

void AsynClient::stop() {
  if (!started_)
    return;
  started_ = false;
  socket_.close();
}

void AsynClient::exec(const Message &query) { write_(query); }

void AsynClient::write_(const Message &query) {
  auto self(shared_from_this());
  xkdb::Response resp;
  if (!dstream_.serialize(query, resp)) {
    stop();
    throw std::runtime_error(resp.emsg());
  }
  boost::asio::async_write( //
      socket_, dstream_.buf(),
      [this, self](boost::system::error_code ec, std::size_t) {
        if (!ec) {
          read_();
        } else {
          stop();
          boost::asio::detail::throw_error(ec);
        }
      });
}

void AsynClient::read_() {
  auto self(shared_from_this());
  boost::asio::async_read_until(
      socket_, dstream_.buf(), DelimitedStream::DELIM,
      [this, self](boost::system::error_code ec, std::size_t length) {
        if (!ec) {
          xkdb::Response resp;
          if (!dstream_.parse(resp, resp, length)) {
            stop();
            throw std::runtime_error(resp.emsg());
          }
          if (!connected_) {
            // first response is always auth response
            if (resp.status() == xkdb::Response::OK) {
              connected_ = true;
            }
          }
          response_handle_(std::move(resp), self);
        } else {
          stop();
          boost::asio::detail::throw_error(ec);
        }
      });
}

} // namespace x_company::xkdbmes
