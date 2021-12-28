CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

PROJECT(ANANAS)

SET(CMAKE_CXX_STANDARD 14)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

set (ROOT /home/larry/myproject/ananas)

add_definitions(-D1)

include_directories(/home/larry/myproject)
include_directories(${ROOT})
include_directories(/usr/local/openssl/include)
link_directories(${ROOT}/install/lib)
link_directories(/usr/local/openssl/include)
link_directories(/usr/local/openssl/lib)


set (sslsrc ${ROOT}/ssl/SSLContext.cc ${ROOT}/ssl/SSLManager.cc)

add_executable(ssl_client ssl_client.cc ${sslsrc} )
target_link_libraries(ssl_client ananas_net ananas_util libssl.a libcrypto.a dl pthread)


add_executable(ssl_server ssl_server.cc ${sslsrc} )
target_link_libraries(ssl_server ananas_net ananas_util libssl.a libcrypto.a dl pthread)