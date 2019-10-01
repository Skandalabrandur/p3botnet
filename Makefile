server:
	g++ --std=c++11 -pthread -Wall server.cpp -o server

client:
	g++ --std=c++11 -pthread -Wall client.cpp -o client

clean:
	rm server client
