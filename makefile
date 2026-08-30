CXX = g++-14
FLAGS = -std=c++23
PORT ?= 5001

all: bin/server.out

bin/server.out: src/server.cpp
	$(CXX) $(FLAGS) src/server.cpp -o bin/server.out

run: bin/server.out
	./bin/server.out $(PORT)

clean:
	rm -rf bin/*.out bin/*.o build/*.out build/*.o
