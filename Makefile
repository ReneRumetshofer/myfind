c = g++
cargs = -Wall -g --std=c++17 -o
target = ./target/

all: build

build: main

main:
	$(c) $(cargs) $(target)main main.cpp

clean:
	rm -rf ./target/*