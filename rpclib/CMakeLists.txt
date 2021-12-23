cmake_minimum_required(VERSION 3.10)

project (rpclib VERSION 1.0)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

find_package(PkgConfig)
pkg_search_module(Proto REQUIRED protobuf)
MESSAGE(STATUS "Proto dirs:" ${Proto_INCLUDE_DIRS})
MESSAGE(STATUS "Proto lib:" ${Proto_LIBRARIES})
include_directories(${Proto_INCLUDE_DIRS})
link_directories(${Proto_LIBRARY_DIRS})

include_directories(
    /home/larry/myproject
    /home/larry/myproject/ananas/sample/rpc/rpclib
    /home/larry/myproject/ananas
)

link_directories(/home/larry/myproject/ananas/install/lib)

set(SRCS
HealthService.cc
ProtobufCoder.cc 
RpcException.cc 
RedisClientContext.cc
RedisProtocol.cc
RpcServer.cc 
RpcService.cc 
RpcServiceStub.cc 
ananas_rpc.pb.cc
  )

# 生成库文件, 基于link muduo_base和源文件
add_library(rpclib ${SRCS})