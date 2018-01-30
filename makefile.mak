client.o: server.o
	g++ -std=c++0x -w   -o  client  client.cpp
server.o: 
	g++ -std=c++0x -w   -o  server  server.cpp
clean:
	rm -rf server
	rm -rf client