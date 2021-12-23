CC=g++
inc=-I /home/larry/myproject 
lib=-L /home/larry/myproject/ananas/install/lib -lananas_rpc -lananas_net -lananas_util `pkg-config --cflags --libs protobuf`

all: server.o
	${CC} -g -o rpcserver test_rpc.pb.o server.o ${lib}
server.o: server.cc test_rpc.pb.cc
	${CC} -g -c server.cc test_rpc.pb.cc ${inc}