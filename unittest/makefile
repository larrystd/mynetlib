CC=g++

lib=/home/larry/myproject/ananas/install/lib -lananas_net -lananas_util -lgtest -lpthread
include=/home/larry/myproject/ananas/install/include

all: Test.o
	${CC} -o ThreadPoolTest ThreadPoolTest.o -L ${lib}

Test.o: BufferTest.cc 
	${CC} -c ThreadPoolTest.cc -I ${include}