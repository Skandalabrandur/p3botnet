all:
	if test -e tsamp3group77; then rm tsamp3group77; fi
	g++ --std=c++11 -pthread -Wall server.cpp fileOperations.cpp messageOperations.cpp -o tsamp3group77
	if test -e client; then rm client; fi
	g++ --std=c++11 -pthread -Wall client.cpp fileOperations.cpp messageOperations.cpp -o client

server:
	if test -e tsamp3group77; then rm tsamp3group77; fi
	g++ --std=c++11 -pthread -Wall server.cpp fileOperations.cpp messageOperations.cpp -o tsamp3group77

compileforskel:
	if test -e tsamp3group77; then rm tsamp3group77; fi
	g++ --std=c++11 -static-libstdc++ -pthread -Wall server.cpp fileOperations.cpp messageOperations.cpp -o tsamp3group77
	if test -e client; then rm client; fi
	g++ --std=c++11 -static-libstdc++ -pthread -Wall client.cpp fileOperations.cpp messageOperations.cpp -o client

client:
	if test -e client; then rm client; fi
	g++ --std=c++11 -pthread -Wall client.cpp -o client

clean:
	if test -e tsamp3group77; then rm tsamp3group77; fi
	if test -e client; then rm client; fi
