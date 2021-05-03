#define BOOST_TEST_MODULE xkdbmes

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "xkdb.pb.h"
#include "xkdbmes.hpp"

namespace utf = boost::unit_test;

using Server = x_company::xkdbmes::Server;
using Session = x_company::xkdbmes::Session;
using Client = x_company::xkdbmes::Client;
using AsynClient = x_company::xkdbmes::AsynClient;

// colorful printing
// yellow begin
const std::string yb = "\033[1;33m";
// yellow end
const std::string ye = "\033[0m";

xkdb::Query sample_query(std::int64_t i) {
  xkdb::Query query;
  query.set_with_merge(false);
  auto event = query.add_events();
  query.set_type(xkdb::Query::INSERT);
  event->set_id(i);
  event->set_device_hash(23452335 + i);
  event->set_device_dt(20210110124425 + i);
  event->set_hide(false);
  event->set_extra("{'some_key': 'some_value'}");
  return query;
}

// define authentication function
auto auth_handle(const xkdb::Auth &auth) {
  return auth.user() == "x-company" && auth.pass() == "123592*123";
}

// define what server should do with a query
auto query_handle(const xkdb::Query &query, xkdb::Response &resp,
                  const x_company::connection_info & /*info*/) {
  for (auto &qevent : query.events()) {
    auto event = resp.add_events();
    event->set_id(qevent.id());
  }
}

BOOST_AUTO_TEST_CASE(xkdb_server) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  boost::asio::io_context ioc;
  int port = 52275;

  Server server(ioc, port, auth_handle, query_handle);

  // create server threads
  boost::thread_group tg;
  tg.create_thread(boost::bind(&boost::asio::io_context::run, &ioc));
  tg.create_thread(boost::bind(&boost::asio::io_context::run, &ioc));

  // give time for threads to start
  boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

  // start client
  boost::asio::io_context cioc;
  Client client(cioc, "127.0.0.1", port);

  // try to connect to a server with a wrong password
  xkdb::Auth wrong_auth;
  wrong_auth.set_user("x-company");
  wrong_auth.set_pass("wrong password");

  auto resp = client.exec(wrong_auth);
  std::cout << yb << "auth response:\n"
            << ye << resp.DebugString() << std::endl;
  BOOST_TEST(resp.status() == xkdb::Response::UNAUTHORIZED);

  // connect to a server with a valid password
  xkdb::Auth auth;
  auth.set_user("x-company");
  auth.set_pass("123592*123");

  resp = client.exec(auth);
  std::cout << yb << "auth response:\n"
            << ye << resp.DebugString() << std::endl;
  BOOST_TEST(resp.status() == xkdb::Response::OK);

  // send a query to a server
  auto query = sample_query(123);

  resp = client.exec(query);
  std::cout << yb << "query response:\n"
            << ye << resp.DebugString() << std::endl;
  BOOST_TEST(resp.status() == xkdb::Response::OK);
  BOOST_TEST(resp.events(0).id() == 123);

  exit(0);

  // wait for server
  tg.join_all();
}

BOOST_AUTO_TEST_CASE(xkdb_client_sync, *utf::disabled()) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  boost::asio::io_context cioc;
  Client client(cioc, "127.0.0.1", 11002);

  // connect to a server with a valid password
  xkdb::Auth auth;
  auth.set_user("x-company");
  auth.set_pass("123592*123");

  xkdb::Response resp;

  // synchronous connect
  resp = client.exec(auth);
  std::cout << yb << "auth response:\n"
            << ye << resp.DebugString() << std::endl;
  BOOST_TEST(resp.status() == xkdb::Response::OK);

  // synchronous query
  auto query = sample_query(123);

  resp = client.exec(query);
  std::cout << yb << "query response:\n"
            << ye << resp.DebugString() << std::endl;
}

BOOST_AUTO_TEST_CASE(xkdb_client_async) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  //================================================================================
  boost::asio::io_context svc;
  int port = 52275;

  Server server(svc, port, auth_handle, query_handle);

  // create server threads
  boost::thread_group tg;
  tg.create_thread(boost::bind(&boost::asio::io_context::run, &svc));
  tg.create_thread(boost::bind(&boost::asio::io_context::run, &svc));

  // give time for threads to start
  boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

  //================================================================================

  boost::asio::io_context ioc;

  // connect to a server with a valid password
  xkdb::Auth auth;
  auth.set_user("x-company");
  auth.set_pass("123592*123");

  // define tasks
  std::queue<xkdb::Query> tasks;
  size_t nids = 0;
  for (size_t i = 0; i < 4; i++) {
    auto query = sample_query(i);
    nids += query.events().size();
    tasks.push(std::move(query));
  }

  std::vector<std::int64_t> ids;

  auto response_handle = [&ids, &tasks](xkdb::Response &&resp,
                                        std::shared_ptr<AsynClient> self) {
    std::cout << self << " " << resp.DebugString() << std::endl;
    // first response is always auth response
    if (!self->connected()) {
      std::cout << "could not connect to server" << std::endl;
      self->stop();
      return;
    }

    // start asynchronous task execution
    if (!tasks.empty()) {
      auto &task = tasks.front();
      self->exec(task);

      for (auto &ev : task.events()) {
        ids.push_back(ev.id());
      }

      tasks.pop();
    }
  };

  // two async clients in a single thread
  auto c1 = AsynClient::start(ioc, "127.0.0.1", port, auth, response_handle);
  auto c2 = AsynClient::start(ioc, "127.0.0.1", port, auth, response_handle);
  ioc.run();

  auto size1 = ids.size();
  std::set<std::int64_t> s(ids.begin(), ids.end());
  BOOST_REQUIRE((size1 == nids) && (size1 == s.size()));

  exit(0);

  // wait for server
  tg.join_all();
}

// launch server for external testing (e.g. for golang)
BOOST_AUTO_TEST_CASE(xkdb_server_listen, *utf::disabled()) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  boost::asio::io_context ioc;
  int port = 52275;

  Server server(ioc, port, auth_handle, query_handle);

  // create server threads
  boost::thread_group tg;
  tg.create_thread(boost::bind(&boost::asio::io_context::run, &ioc));
  tg.create_thread(boost::bind(&boost::asio::io_context::run, &ioc));

  // wait for server
  tg.join_all();
}
