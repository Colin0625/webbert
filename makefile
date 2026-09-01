CXX = g++-14
FLAGS = -std=c++23 -Iinclude
PORT ?= 5001

all: bin/main.out

build/server.o: include/server.hpp src/server.cpp
	$(CXX) $(FLAGS) -c src/server.cpp -o build/server.o

bin/main.out: src/main.cpp build/server.o
	$(CXX) $(FLAGS) build/server.o src/main.cpp -o bin/main.out

run: bin/main.out
	./bin/main.out $(PORT)

clean:
	rm -rf bin/*.out bin/*.o build/*.out build/*.o
