cmake_minimum_required(VERSION 3.10)

project (rpclib VERSION 1.0)


SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

include_directories(/usr/local/include)
link_directories(/usr/local/lib)


set(SRCS
rpc/HealthService.cc
rpc/ProtobufCoder.cc 
rpc/RpcException.cc 
rpc/RedisClientContext.cc
rpc/RedisProtocol.cc
rpc/RpcServer.cc 
rpc/RpcService.cc 
rpc/RpcServiceStub.cc 
rpc/ananas_rpc.pb.cc
  )

include_directories(
    /home/larry/myproject
    /home/larry/myproject/ananas/rpclib
    /home/larry/myproject/ananas
)

link_directories(/home/larry/myproject/ananas/install/lib)
#link_directories(/home/larry/myproject/ananas/rpclib/build)

add_executable(rpcdemo  server.cc test_rpc.pb.cc ${SRCS})
target_link_libraries(rpcdemo  libprotobuf.a ananas_net ananas_util pthread)