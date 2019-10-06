//
// Simple chat server for TSAM-409
//
// Command line: ./chat_server 4000
//
// Base:        Author: Jacky Mallett (jacky@ru.is)
//  -The base server can be seen in its original form under /root/base/server.cpp
//
// Implement:   Authors:    Fjóla Sif Sigvaldadóttir (fjola17@ru.is)
//                          Þorsteinn Sævar Kristjánsson (thorsteinnk17@ru.is)
//
//
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <vector>

#include <iostream>
#include <sstream>
#include <thread>
#include <map>

#include <unistd.h>

#include <boost/circular_buffer.hpp>
#include <boost/algorithm/string.hpp>


#include "fileOperations.h"
#include "messageOperations.h"
#include "dataObjects.h"

// fix SOCK_NONBLOCK for OSX
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#define BACKLOG  5          // Allowed length of queue of waiting connections

// Simple class for handling connections from clients.
//
// Client(int socket) - socket to send/receive traffic from client.
class Client
{
public:
    int sock;              // socket of client connection
    std::string name;           // Limit length of name of client's user

    Client(int socket) : sock(socket) {}

    ~Client() {}           // Virtual destructor defined for base class
};

// Note: map is not necessarily the most efficient method to use here,
// especially for a server with large numbers of simulataneous connections,
// where performance is also expected to be an issue.
//
// Quite often a simple array can be used as a lookup table,
// (indexed on socket no.) sacrificing memory for speed.

std::map<int, Client*> clients;     // Lookup table for per Client information
std::map<int, Server*> servers;   // Lookup table for connected servers

typedef boost::circular_buffer<s_message> circular_buffer;
circular_buffer cb{500};



// Open socket for specified port.
//
// Returns -1 if unable to create the socket for any reason.

int open_socket(int portno)
{
    struct sockaddr_in sk_addr;   // address settings for bind()
    int sock;                     // socket opened for this port
    int set = 1;                  // for setsockopt

    // Create socket for connection. Set to be non-blocking, so recv will
    // return immediately if there isn't anything waiting to be read.
#ifdef __APPLE__
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Failed to open socket");
        return(-1);
    }
#else
    if((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
        perror("Failed to open socket");
        return(-1);
    }
#endif

    // Turn on SO_REUSEADDR to allow socket to be quickly reused after
    // program exit.

    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0) {
        perror("Failed to set SO_REUSEADDR:");
    }

#ifdef __APPLE__
    if(setsockopt(sock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0) {
        perror("Failed to set SOCK_NOBBLOCK");
    }
#endif
    memset(&sk_addr, 0, sizeof(sk_addr));

    sk_addr.sin_family      = AF_INET;
    sk_addr.sin_addr.s_addr = INADDR_ANY;
    sk_addr.sin_port        = htons(portno);

    // Bind to socket to listen for connections from clients

    if(bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0) {
        perror("Failed to bind to socket:");
        return(-1);
    } else {
        return(sock);
    }
}

// Close a client's connection, remove it from the client list, and
// tidy up select sockets afterwards.

void closeClient(int clientSocket, fd_set *openSockets, int *maxfds)
{
    // Remove client from the clients list
    clients.erase(clientSocket);

    // If this client's socket is maxfds then the next lowest
    // one has to be determined. Socket fd's can be reused by the Kernel,
    // so there aren't any nice ways to do this.

    if(*maxfds == clientSocket) {
        for(auto const& p : servers) {
            *maxfds = std::max(*maxfds, p.second->sock);
        }
        for(auto const& p : clients) {
            *maxfds = std::max(*maxfds, p.second->sock);
        }
    }

    // And remove from the list of open sockets.

    FD_CLR(clientSocket, openSockets);
}

void closeServer(int serverSocket, fd_set *openSockets, int *maxfds) {
    servers.erase(serverSocket);

    if(*maxfds == serverSocket) {
        for(auto const& p : clients) {
            *maxfds = std::max(*maxfds, p.second->sock);
        }
        for(auto const& p : servers) {
            *maxfds = std::max(*maxfds, p.second->sock);
        }
    }

    FD_CLR(serverSocket, openSockets);
}

// Process command from client on the server

void clientCommand(int clientSocket, fd_set *openSockets, int *maxfds,
                   char *buffer) {
    std::string msg = extractMessage((std::string) buffer);

    std::vector<std::string> strs;
    boost::split(strs,msg,boost::is_any_of(","));

    std::cout << "strs.size() = " << strs.size() << std::endl;

    if(strs.size() > 0) {
        if(strs[0] == "GETMSG") {
            if(strs.size() == 2) {
                std::cout << "Received GETMSG command" << std::endl;
                std::cout << "ARGUMENT: " << strs[1] << std::endl;
            } else {
                std::string g_msg = "Only one argument for GETMSG! You supplied too many!";
                send(clientSocket, g_msg.c_str(), g_msg.length(), 0);
            }
        } else if (strs[0] == "SENDMSG") {
            if(strs.size() == 2) {
                std::cout << "Received SENDMSG command" << std::endl;
                std::cout << "ARGUMENT: " << strs[1] << std::endl;
            } else {
                std::string s_msg = "Only one argument for GETMSG! You supplied too many!";
                send(clientSocket, s_msg.c_str(), s_msg.length(), 0);
            }
        } else if(boost::contains(strs[0], (std::string) "LISTSERVERS")) {
            std::cout << "Received LISTSERVERS command" << std::endl;
        }
    }

    std::cout << "Received buffer: " << buffer << std::endl;


}

void serverCommand(int serverSocket, fd_set *openSockets, int *maxfds,
        char *buffer) {
    std::string msg = extractMessage((std::string) buffer);

    std::vector<std::string> strs;
    boost::split(strs,msg,boost::is_any_of(","));

    if(strs.size() > 0) {
        if(strs[0] == "LISTSERVERS") {
            if(strs.size() == 2) {
                std::cout << "Received LISTSERVERS command" << std::endl;
                std::cout << "ARGUMENT: " << strs[1] << std::endl;
            } else {
                std::string ls_msg = "Only one argument for LISTSERVERS! You supplied too many!";
                send(serverSocket, ls_msg.c_str(), ls_msg.length(), 0);
            }
        } else if (strs[0] == "KEEPALIVE") {
            std::cout << "Received KEEPALIVE command" << std::endl;
        } else if (strs[0] == "GET_MSG") {
            std::cout << "Received GET_MSG command" << std::endl;
        } else if (strs[0] == "SEND_MSG") {
            std::cout << "Received LEAVE command" << std::endl;
        } else if (strs[0] == "STATUSREQ") {
            std::cout << "Received STATUSREQ command" << std::endl;
        } else if (strs[0] == "STATUSRESP") {
            std::cout << "Received STATUSRESP command" << std::endl;
        } else {
            std::string e_msg = "Please refer to the pdf on how to send messages.\nDid you perhaps forget a comma in LISTSERVERS or another command? LISTSERVERS on its own is not enough. LISTERVERS,YOUR_GROUP_ID is the way to go my friend!";

            send(serverSocket, e_msg.c_str(), e_msg.length()-1, 0);
        }
    }

    std::cout << "Received from server:\n\t" << msg << std::endl;

}

int main(int argc, char* argv[])
{
    bool finished;
    int clistenSock;                // Socket for client connections to server
    int clientSock;                 // Socket of connecting client
    int slistenSock;                // Socket for server connections to server
    int serverSock;                 // Socket of connected servers
    fd_set openSockets;             // Current open sockets
    fd_set readSockets;             // Socket list for select()
    fd_set exceptSockets;           // Exception socket list

    fd_set openServerSockets;
    fd_set readServerSockets;
    int maxfds;                     // Passed to select() as max fd in set
    struct sockaddr_in client;
    socklen_t clientLen;

    struct sockaddr_in server;
    socklen_t serverLen;
    char buffer[1025];              // buffer for reading from clients

    if(argc != 5) {
        //SERVERS and CLIENTS are mainly used to make it readable for people searching for servers
        //via the 'w' command
        printf("Usage: chat_server SERVERS <server listen port> CLIENTS <client listen port>\n");
        exit(0);
    }

    // Setup socket for server to listen to

    clistenSock = open_socket(atoi(argv[4]));
    printf("Listening for clients on port: %d\n", atoi(argv[4]));

    slistenSock = open_socket(atoi(argv[2]));

    printf("Listening for server on port: %d\n", atoi(argv[2]));

    if(listen(clistenSock, BACKLOG) < 0) {
        printf("Listening for clients failed on port %s\n", argv[4]);
        exit(0);
    } else
        // Add listen socket to socket set we are monitoring
    {
        FD_SET(clistenSock, &openSockets);
        maxfds = clistenSock;
    }

    if(listen(slistenSock, BACKLOG) < 0) {
        printf("Listening for servers failed on port %s\n", argv[2]);
        exit(0);
    } else {
        FD_SET(slistenSock, &openSockets);
        maxfds = slistenSock;        //TODO: determine maxfds in a better way?
    }


    finished = false;

    while(!finished) {
        

        // Get modifiable copy of readSockets
        readSockets = exceptSockets = openSockets;
        memset(buffer, 0, sizeof(buffer));

        // Look at sockets and see which ones have something to be read()
        int n = select(maxfds + 1, &readSockets, NULL, &exceptSockets, NULL);

        if(n < 0) {
            perror("select failed - closing down\n");
            finished = true;
        } else {
            // First, accept  any new connections to the server on the listening socket

            if(FD_ISSET(clistenSock, &readSockets)) {
                clientLen = sizeof(client);
                clientSock = accept(clistenSock, (struct sockaddr *)&client,
                                    &clientLen);

                // Add new client to the list of open sockets
                FD_SET(clientSock, &openSockets);

                // And update the maximum file descriptor
                maxfds = std::max(maxfds, clientSock);

                // create a new client to store information.
                clients[clientSock] = new Client(clientSock);

                // Decrement the number of sockets waiting to be dealt with
                n--;

                printf("Client connected on server: %d\n", clientSock);
                std::ostringstream ss;
                ss << "Client connected on server: " << clientSock << std::endl;
                writeToLog(ss.str());
            }

            if(FD_ISSET(slistenSock, &readSockets)) {
                serverLen = sizeof(server);
                serverSock = accept(slistenSock, (struct sockaddr *)&server,
                                    &serverLen);

                // Add new client to the list of open sockets
                FD_SET(serverSock, &openSockets);

                // And update the maximum file descriptor
                maxfds = std::max(maxfds, serverSock);

                // create a new client to store information.
                servers[serverSock] = new Server(serverSock);

                // Decrement the number of sockets waiting to be dealt with
                n--;

                printf("Server connected on server: %d\n", serverSock);
                std::ostringstream ss;
                ss << "Server connected on server: " << serverSock << std::endl;
                writeToLog(ss.str());
            }
 
            // Now check for commands from clients
            while(n-- > 0) {
                for(auto const& pair : servers) {
                    Server *server = pair.second;

                    if(FD_ISSET(server->sock, &readSockets)) {
                        // recv() == 0 means client has closed connection
                        if(recv(server->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0) {
                            printf("Server closed connection: %d", server->sock);
                            close(server->sock);

                            closeServer(server->sock, &openSockets, &maxfds);

                        }
                        // We don't check for -1 (nothing received) because select()
                        // only triggers if there is something on the socket for us.
                        else {
                            std::cout << buffer << std::endl;
                            serverCommand(server->sock, &openSockets, &maxfds,
                                          buffer);
                        }
                    }
                }

                for(auto const& pair : clients) {
                    Client *client = pair.second;

                    if(FD_ISSET(client->sock, &readSockets)) {
                        // recv() == 0 means client has closed connection
                        if(recv(client->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0) {
                            printf("Client closed connection: %d", client->sock);
                            close(client->sock);

                            closeClient(client->sock, &openSockets, &maxfds);

                        }
                        // We don't check for -1 (nothing received) because select()
                        // only triggers if there is something on the socket for us.
                        else {
                            std::cout << buffer << std::endl;
                            clientCommand(client->sock, &openSockets, &maxfds,
                                          buffer);
                        }
                    }
                }
            }
        }
    }
}
