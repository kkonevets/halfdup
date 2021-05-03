cmake_minimum_required(VERSION 3.13)
project(xkdb_message)
enable_testing()

find_package(Boost REQUIRED COMPONENTS system filesystem log log_setup unit_test_framework thread)
find_package(PQXX REQUIRED)
find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ../proto/xkdb/xkdb.proto)

add_library(libxkdb SHARED server.cpp client.cpp dstream.cpp ${PROTO_SRCS} ${PROTO_HDRS})

add_executable(test_libxkdb test.cpp ${PROTO_SRCS} ${PROTO_HDRS})

target_include_directories(libxkdb PUBLIC ./../libxcompany_core)

target_link_libraries(libxkdb PRIVATE
  ${Boost_LIBRARIES}
  ${Protobuf_LIBRARIES}
)

target_link_libraries(test_libxkdb PRIVATE
  libxkdb
  ${Boost_LIBRARIES}
  ${Protobuf_LIBRARIES}
)

add_test(NAME test_libxkdb COMMAND test_libxkdb)
INSTALL(TARGETS LIBRARY DESTINATION ${LIB_INSTALL_DIR})
INSTALL(FILES xkdbmes.hpp DESTINATION ${INCLUDE_INSTALL_DIR}/x-company)
