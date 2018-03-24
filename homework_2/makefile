
all: client.cpp
	g++ -Wall -g -pedantic -lpthread client.cpp

server: kareludp-server
	./kareludp-server 1000 - png.png firmware-karel-1.6.0.bin

run_cli: all
	./a.out 127.0.0.1

run_srv: all
	./a.out 127.0.0.1 firmware-karel-1.6.0.bin
