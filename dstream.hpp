// "Copyright 2021 Kirill Konevets"

/**
 *   \file dstream.hpp
 *   \brief protobuf serialization/parsing to/from Boost.Asio stream buffer
 */

#pragma once

#include <boost/asio.hpp>
#include <boost/asio/write.hpp>
#include <google/protobuf/message.h>
#include <string>

#include <xkdb.pb.h>

using Message = google::protobuf::Message;

namespace x_company::xkdbmes {

/**
 *  \brief copy bytes from stream buffer to a string
 *  \param length number of bytes to copy
 *  \return substring of stream buffer
 */
std::string streambuf_copy(boost::asio::streambuf &sb, size_t length);

/**
 *  \brief Incapsulates socket io with a delimiter
 */
class DelimitedStream {
public:
  static constexpr std::string_view DELIM = "==DELIM==";

  /**
   *  \param error_status Status to set on error
   */
  explicit DelimitedStream(xkdb::Response::Status error_status)
      : error_status_(error_status) {}

  /**
   *  \brief Serialize query into socket ostream and add delimiter
   *
   *  \return true if serialized sucessfully, otherwise sets response status and
   * error message
   */
  bool serialize(const Message &query, xkdb::Response &resp);

  /**
   *  \brief Parse query from socket delimited istream
   *
   *  \param length The number of bytes in the streambuf's get area up to and
   * including the delimiter
   * \return true if parsed sucessfully, otherwise sets response status and
   * error message
   */
  bool parse(Message &query, xkdb::Response &resp, size_t length);

  /**
   *  Get socket stream buffer
   */
  boost::asio::streambuf &buf() { return sb_; }

private:
  boost::asio::streambuf sb_;
  std::istream is_{&sb_};
  std::ostream os_{&sb_};
  xkdb::Response::Status error_status_;
};

} // namespace x_company::xkdbmes
