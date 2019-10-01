server:
	g++ --std=c++11 -Wall server.cpp -o server

client:
	g++ --std=c++11 -pthread -Wall client.cpp -o client
