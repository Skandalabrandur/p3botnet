all:
	if test -e server; then rm server; fi
	g++ --std=c++11 -pthread -Wall server.cpp -o server
	if test -e client; then rm client; fi
	g++ --std=c++11 -pthread -Wall client.cpp -o client

server:
	if test -e server; then rm server; fi
	g++ --std=c++11 -pthread -Wall server.cpp fileOperations.cpp messageOperations.cpp -o server

compileforskel:
	if test -e server; then rm server; fi
	g++ --std=c++11 -static-libstdc++ -pthread -Wall server.cpp -o server
	if test -e client; then rm client; fi
	g++ --std=c++11 -static-libstdc++ -pthread -Wall client.cpp -o client

client:
	if test -e client; then rm client; fi
	g++ --std=c++11 -pthread -Wall client.cpp -o client

clean:
	if test -e server; then rm server; fi
	if test -e client; then rm client; fi
