CC=g++
inc=-I /home/larry/myproject
lib=-L /home/larry/myproject/ananas/install/lib -lananas_net -lananas_util `pkg-config --cflags --libs protobuf`

all: server.o
	${CC} -o rpcserver server.o ${lib}
server.o: server.cc test_rpc.pb.cc
	${CC} -c server.cc test_rpc.pb.cc ${inc}