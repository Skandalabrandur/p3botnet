gpp             = g++ --std=c++11 -pthread -Wall
skelAddendum    = -static-libstdc++
serverDirective = server.cpp fileOperations.cpp messageOperations.cpp ip.cpp -o tsamp3group77
clientDirective = client.cpp messageOperations.cpp fileOperations.cpp -o client

all:
	if test -e tsamp3group77; then rm tsamp3group77; fi
	$(gpp) $(serverDirective)
	if test -e client; then rm client; fi
	$(gpp) $(clientDirective)

server:
	if test -e tsamp3group77; then rm tsamp3group77; fi
	$(gpp) $(serverDirective)

compileforskel:
	if test -e tsamp3group77; then rm tsamp3group77; fi
	$(gpp) $(skelAddendum) $(serverDirective)
	if test -e client; then rm client; fi
	$(gpp) $(skelAddendum) $(clientDirective)

client:
	if test -e client; then rm client; fi
	$(gpp) $(clientDirective)

clean:
	if test -e tsamp3group77; then rm tsamp3group77; fi
	if test -e client; then rm client; fi
