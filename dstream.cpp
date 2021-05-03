#include "dstream.hpp"

namespace x_company::xkdbmes {

std::string streambuf_copy(boost::asio::streambuf &sb, size_t length) {
  return std::string(boost::asio::buffers_begin(sb.data()),
                     boost::asio::buffers_begin(sb.data()) + length);
}

bool DelimitedStream::serialize(const Message &query, xkdb::Response &resp) {
  bool success = query.SerializeToOstream(&os_);
  if (!success) {
    resp.Clear();
    resp.set_status(error_status_);
    resp.set_emsg(std::string("could not serialize ") + typeid(query).name());
  } else {
    os_ << DELIM;
  }
  return success;
}

bool DelimitedStream::parse(Message &query, xkdb::Response &resp,
                            size_t length) {
  // get next `length` bytes from a scoket excluding delimiter
  auto subs = streambuf_copy(sb_, length - DELIM.size());
  // Consume through the first delimiter.
  sb_.consume(length);
  bool success = query.ParseFromString(subs);
  if (!success) {
    resp.Clear();
    resp.set_status(error_status_);
    resp.set_emsg(std::string("could not parse ") + typeid(query).name());
  }
  return success;
}

} // namespace x_company::xkdbmes
