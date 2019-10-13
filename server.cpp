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
#include <typeinfo>

#include <iostream>
#include <sstream>
#include <map>

#include <unistd.h>

#include <boost/circular_buffer.hpp>
#include <boost/algorithm/string.hpp>

#include <chrono>
#include <ctime>


#include "fileOperations.h"
#include "messageOperations.h"
#include "dataObjects.h"
#include "ip.h"

// fix SOCK_NONBLOCK for OSX
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#define BACKLOG  5              // Allowed length of queue of waiting connections
#define MYGROUP "P3_GROUP_77"   // Our group id

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


// We are using boost to create a circular buffer object
// we define the type and then initialize it below
typedef boost::circular_buffer<s_message> circular_buffer;
circular_buffer message_buffer{500};

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

// Takes in address and port to the server and connects to it
// It requires the openSockets and maxfds to manage socket file descriptors
// It lastly requires our own port because we pre-emptively send a SERVERS response with
// our server so that if other servers fail to ask for LISTSERVERS they will still receive
// information about us
int connectToServer(char *address, char *port, char *our_port, fd_set *openSockets, int *maxfds) {

    // Stolen from client.cpp
    struct addrinfo hints, *svr;              // Network host entry for server
    //struct sockaddr_in server_addr;           // Socket address for server
    int serverSocket;                         // Socket used for server
    int set = 1;                              // Toggle for setsockopt

    hints.ai_family   = AF_INET;            // IPv4 only addresses
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;
    memset(&hints,   0, sizeof(hints));


    if(getaddrinfo(address, port, &hints, &svr) != 0) {
        perror("getaddrinfo failed: ");
        return -1;
    }

    serverSocket = socket(svr->ai_family, svr->ai_socktype, svr->ai_protocol);

    // Turn on SO_REUSEADDR to allow socket to be quickly reused after
    // program exit.

    if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0) {
        printf("Failed to set SO_REUSEADDR for port %s\n", port);
        perror("setsockopt failed: ");
        return -1;
    }

    if(connect(serverSocket, svr->ai_addr, svr->ai_addrlen )< 0) {
        printf("Failed to open socket to server: %s\n", address);
        perror("Connect failed: ");
        return -1;
    }
    // Send our server information as soon as a connection is made
    std::string myIp = getOwnIp();
    std::ostringstream response_msg;
    response_msg << "SERVERS," << MYGROUP << "," << myIp << "," << our_port << ";";
    std::string response = constructMessage(response_msg.str());

    response_msg << " | SENT TO SERVER" << std::endl;
    writeToLog(response_msg.str());
    send(serverSocket, response.c_str(), response.length(), 0);

    // Add new client to the list of open sockets
    FD_SET(serverSocket, openSockets);

    // And update the maximum file descriptor
    *maxfds = std::max(*maxfds, serverSocket);

    // create a new client to store information.
    if(servers.size() < 5) {
        servers[serverSocket] = new Server(serverSocket, (std::string) address);
        servers[serverSocket]->port = atoi(port);
    }

    printf("Connecting to: %d\n", serverSocket);
    std::ostringstream ss;
    ss << "RECEIVED FROM CLIENT CONNECT COMMAND and am connecting to: " << address << ":" << port << std::endl;
    writeToLog(ss.str());
    return serverSocket;
}

// The main function has a timeout via the chronos construct
// that waits at least 60 seconds between timedTasks calls
// The select timeout is set to 1 second so that this time constraint
// is checked every second
void timedTasks() {
    // For all servers who have announced their group_id
    // check if they have any messages and send the corresponding KEEPALIVE
    for(auto const& p : servers) {
        if (strcmp(p.second->group_id.c_str(), "UNKNOWN") == 0) {
            continue;
        }

        int unread_messages = 0;
        for(unsigned long int i = 0; i < message_buffer.size(); i++) {
            s_message tmp_msg = message_buffer[i];
            if(strcmp(p.second->group_id.c_str(), tmp_msg.receiver.c_str()) == 0 && tmp_msg.unread) {
                unread_messages += 1;
            }
        }
        // Construct and send the KEEPALIVE message
        std::ostringstream ka_msg;
        ka_msg << "KEEPALIVE," << unread_messages << std::endl;
        std::string ka = constructMessage(ka_msg.str());
        send(p.second->sock, ka.c_str(), ka.length(), 0);
    }
}

// erases the server from the opensockets and the servers map
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

/////////////////////////////////////////////////////////////////////
//CLIENT COMMANDS
////////////////////////////////////////////////////////////////////

int clientCommand(int clientSocket, fd_set *openSockets, int *maxfds,
        char *buffer, char* our_port) {
    std::string msg = extractMessage((std::string) buffer);

    //If the message is invalid, then don't try to do something
    if(msg.length() == 0) {
        return 0;
    }

    // Split the incoming buffer by commas and newlines
    std::vector<std::string> strs;
    boost::split(strs,msg,boost::is_any_of(",\n"));
    strs.pop_back();    //drop the newline


    // If any tokens are received
    if(strs.size() > 0) {
        std::ostringstream msgFromClient;
        msgFromClient << "Received from client on socket: ";
        msgFromClient << clientSocket << "\n\t" << msg << std::endl;
        writeToLog(msgFromClient.str());

        //Issued a connect command to server
        if(strs[0] == "CONNECT") {
            if(strs.size() == 3) {
                // Connect to the server using the parameters from the CONNECT command
                int newServerSock = connectToServer((char *) strs[1].c_str(), 
                        (char *) strs[2].c_str(), our_port,  openSockets, maxfds);

                // If connection is successful
                if(newServerSock != -1) {
                    writeToLog("Sent SUCCESS back to client on successful connection");
                    send(clientSocket, "SUCCESS", 7, 0);

                    // This procs a LISTSERVERS from the new connected server
                    // in order to ascertain its info
                    std::ostringstream request;
                    request << "LISTSERVERS," << MYGROUP;
                    std::string crequest = constructMessage(request.str());
                    send(newServerSock, crequest.c_str(), crequest.length(), 0);
                } else {
                    writeToLog("Sent FAIL back to client on unsuccessful connection attempt");
                    send(clientSocket, "FAIL", 4, 0);
                }
            }
        } else if(strs[0] == "DISCONNECT") {
            // Returning -1 is used in a check inside an iterator
            // for the clients. The clientsocket is pushed onto 
            // vector that is later used to disconnect
            writeToLog("Client issued Disconnect command");
            return -1;
        } else if(strs[0] == "GETMSG") {
            if(strs.size() == 2) {
                std::cout << "Received GETMSG command" << std::endl;
                std::cout << "ARGUMENT: " << strs[1] << std::endl;

                // Predefine error message. Overwritten if something is found
                std::string gotten_message = "No message found. Take heed that group_id is case sensitive and doesn't trim spaces";

                // Go through the message buffer and find the first occurrence
                // of an unread message for the group id
                for(unsigned long int i = 0; i < message_buffer.size(); i++) {
                    s_message tmp_msg = message_buffer[i];
                    if(strcmp(tmp_msg.receiver.c_str(), strs[1].c_str()) == 0
                            && tmp_msg.unread) {
                        gotten_message = tmp_msg.msg;
                        message_buffer[i].unread = false;
                        break;
                    }
                }

                std::ostringstream clientLog;
                clientLog << "Following message sent to client: ";
                clientLog << gotten_message;
                writeToLog(clientLog.str());
                send(clientSocket, gotten_message.c_str(), gotten_message.length(), 0);

            } else {
                std::string g_msg = "Only one argument for GETMSG! You supplied too many!";
                writeToLog("Sent an error message to client because getmsg was ill formed");
                send(clientSocket, g_msg.c_str(), g_msg.length(), 0);
            }
        } else if (strs[0] == "SENDMSG") {
            if(strs.size() > 2) {
                std::cout << "Received SENDMSG command" << std::endl;
                std::cout << "GROUP_ID ARGUMENT: " << strs[1] << std::endl;
                s_message new_msg_struct;
                std::ostringstream new_msg;

                // Join old message again with commas, since we split arguments
                // that way. If the original message had commas, this is where
                // they return
                for(long unsigned int i = 2; i < strs.size() - 1; i++) {
                    new_msg << strs[i] << ",";
                }
                new_msg << strs[strs.size() - 1];

                new_msg_struct.msg = new_msg.str();
                new_msg_struct.sender = MYGROUP;  // Who is client? He is us!
                new_msg_struct.receiver = strs[1];
                new_msg_struct.unread = true;
                message_buffer.push_back(new_msg_struct);

            } else {
                std::string s_msg = "You need to supply a group AND a message with SENDMSG,GROUP,MSG!";
                writeToLog("Sent an error message regarding SENDMSG to client since it was ill formed");
                send(clientSocket, s_msg.c_str(), s_msg.length(), 0);
            }
        } else if(boost::contains(strs[0], (std::string) "LISTSERVERS")) {
            std::cout << "Received LISTSERVERS command" << std::endl;
            std::string myIp = getOwnIp();
            std::ostringstream response;

            // Send our information
            response << "SERVERS,"; //<< MYGROUP << "," << myIp << "," << ";";

            // Sends back valid connected servers, those who have a valid port and a valid group_id
            for(auto const& p : servers) {
                if(strcmp(p.second->group_id.c_str(), "UNKNOWN") != 0 && p.second->port != -1) {
                    response << p.second->group_id << ",";
                    response << p.second->address << ",";
                    response << p.second->port;
                    response << ";";
                }
            }

            std::string response_msg = constructMessage(response.str());
            response << " | SENT TO CLIENT" << std::endl;
            writeToLog(response.str());
            send(clientSocket, response_msg.c_str(), response_msg.length(), 0);
        } else if(boost::contains(strs[0], (std::string) "LISTMESSAGES")) {
            std::ostringstream messages;
            int howManyMessages = 0;

            // Iterate through all messages and show them
            // Mainly used as a debug command
            for(unsigned long int i = 0; i < message_buffer.size(); i++) {
                s_message tmp_msg = message_buffer[i];
                messages << "Sender: \t" << tmp_msg.sender << std::endl;
                messages << "Recevr: \t" << tmp_msg.receiver << std::endl;
                messages << "Messag: \t" << tmp_msg.msg << std::endl;
                messages << "unread: \t" << tmp_msg.unread << std::endl;
                messages << std::endl << std::endl;
                howManyMessages += 1;
            }

            std::string response = constructMessage(messages.str());
            std::ostringstream clientLog;
            clientLog << "Sent a list of " << howManyMessages;
            clientLog << " to client (truncated the list) " << std::endl;
            writeToLog(clientLog.str());
            send(clientSocket, response.c_str(), response.length(), 0);
        }
    }
    memset(&buffer, 0, sizeof(buffer));
    return 0;
}

////////////////////////////////////////////////////////////////////
//SERVER COMMANDS
////////////////////////////////////////////////////////////////////

void serverCommand(int serverSocket, fd_set *openSockets, int *maxfds,
        char *buffer, char *serverListenPort) {
    std::string msg = extractMessage((std::string) buffer);

    // If message is of an invalid length, then do nothing
    if(msg.length() == 0) {
        return;
    }

    // Split the message by commas
    std::vector<std::string> strs;
    boost::split(strs,msg,boost::is_any_of(","));


    // If there are tokens to process then
    if(strs.size() > 0) {
        std::ostringstream msgFromServer;
        msgFromServer << "Received from server " << servers[serverSocket]->group_id;
        msgFromServer << " on socket: " << serverSocket << "\n\t" << msg << std::endl;
        writeToLog(msgFromServer.str());

        if(strs[0] == "LISTSERVERS") {
            if(strs.size() == 2) {

                std::string myIp = getOwnIp();
                std::ostringstream response;

                // Send information about our own server
                response << "SERVERS," << MYGROUP << "," << myIp << "," << serverListenPort << ";";

                // Send information about all valid servers connected to our server
                // those who have a valid port and a valid group_id
                for(auto const& p : servers) {
                    if(strcmp(p.second->group_id.c_str(), "UNKNOWN") != 0 && p.second->port != -1) {
                        response << p.second->group_id << ",";
                        response << p.second->address << ",";
                        response << p.second->port;
                        response << ";";
                    }
                }

                std::string response_msg = constructMessage(response.str());

                response << " | SENT TO SERVER" << std::endl;
                writeToLog(response.str());
                
                send(serverSocket, response_msg.c_str(), response_msg.length(), 0);

            } else {
                writeToLog("Someone sent LISTSERVERS with too many arguments!");
                std::string ls_msg = "Only one argument for LISTSERVERS! You supplied too many!";
                send(serverSocket, ls_msg.c_str(), ls_msg.length(), 0);
            }
        } else if (strs[0] == "SERVERS") {
            // split incoming servers message by ;'s
            std::vector<std::string> server_infos;
            boost::split(server_infos,msg,boost::is_any_of(";"));

            // Initial server we are already connected to
            // We update the information relevant to us
            // To be implemented: Iterate through the remainder and automatically connect
            if(server_infos.size() > 0) {
                std::vector<std::string> server_info;
                boost::split(server_info,server_infos[0],boost::is_any_of(",\n"));
                if(server_info.size() == 4) {
                    // Getting Group_id
                    servers[serverSocket]->group_id = server_info[1];
                    servers[serverSocket]->address = server_info[2];
                    servers[serverSocket]->port = atoi(server_info[3].c_str());
                }
            }

        } else if (strs[0] == "KEEPALIVE") {
            // Erase newlines from the second token, if they are present
            strs[1].erase(std::remove(strs[1].begin(), 
                            strs[1].end(), '\n'), strs[1].end());

            std::cout << "Received KEEPALIVE command" << std::endl;

            // Convert the number of messages argument to an integer
            int numOfMessages = atoi(strs[1].c_str());

            // Get ONE message at a time from each server if they have something for us
            // prevents spam
            if(numOfMessages > 0) {
                std::ostringstream response;
                response << "GET_MSG," << MYGROUP;
                std::string response_msg = constructMessage(response.str());
                response << " | SENT TO SERVER FROM KEEPALIVE,(num greater than 0)" << std::endl;
                send(serverSocket, response_msg.c_str(), response_msg.length(), 0);
            }
        } else if (strs[0] == "GET_MSG") {
            if(strs.size() == 2) {
                std::cout << "Received GET_MSG command with GROUP_ID: " << strs[1] << std::endl;

                // Erase newlines from the last argument if it is present
                strs[1].erase(std::remove(strs[1].begin(), 
                            strs[1].end(), '\n'), strs[1].end());

                // Find a server with a matching group id
                for(auto const& p: servers) {
                    if(strcmp(p.second->group_id.c_str(), strs[1].c_str()) == 0) {
                        std::ostringstream gm;
                        std::string gotten_message = "";

                        // Go through the message_buffer. If a message that is unread
                        // is found for this GROUP_ID in the request, it is sent back
                        // in a correct SEND_MSG format to the requesting server
                        for(unsigned long int i = 0; i < message_buffer.size(); i++) {
                            s_message tmp_msg = message_buffer[i];
                            if(strcmp(tmp_msg.receiver.c_str(), strs[1].c_str()) == 0
                                    && tmp_msg.unread) {
                                gotten_message = tmp_msg.msg;
                                message_buffer[i].unread = false;
                                gm << "SEND_MSG," << MYGROUP << "," << strs[1] << "," << gotten_message;
                                std::string response = constructMessage(gm.str());

                                gm << " | SENT TO SERVER " << std::endl;
                                writeToLog(gm.str());

                                send(serverSocket, response.c_str(), response.length(), 0);
                                break;
                            }
                        }
                    }
                }
            }
        } else if (strs[0] == "SEND_MSG") {
            if(strs.size() >= 4) { 
                s_message new_msg_struct;
                std::ostringstream new_msg;

                // Rejoin our tokens on commas to rebuild the 
                // message, if necessary
                for(long unsigned int i = 3; i < strs.size() - 1; i++) {
                    new_msg << strs[i] << ",";
                }
                new_msg << strs[strs.size() - 1];

                if(new_msg.str().length() > 0) {
                    new_msg_struct.msg = new_msg.str();
                    new_msg_struct.sender = strs[1];
                    new_msg_struct.receiver = strs[2];
                    new_msg_struct.unread = true;
                    message_buffer.push_back(new_msg_struct);
                }
        } else if (strs[0] == "LEAVE") {
            std::cout << "Received LEAVE command" << std::endl;
            if(strs.size() == 3) {
                int closedServer = -1;
                // Go through the servers and stage it for a disconnect
                // if it is found
                for(auto const& p: servers) {
                    // If address and port are found in the servers structure
                    if(strcmp(p.second->address.c_str(), strs[1].c_str()) == 0
                            && p.second->port == atoi(strs[2].c_str())) {
                        closedServer = p.second->sock;
                    }
                }

                if(closedServer != -1) {
                    close(closedServer);
                    closeServer(closedServer, openSockets, maxfds);
                }
            }
        } else if (strs[0] == "STATUSREQ") {
            if (strs.size() == 2) {
                std::cout << "Received STATUSREQ command" << std::endl;

                // Erase the newline on the last token if present
                strs[1].erase(std::remove(strs[1].begin(), 
                            strs[1].end(), '\n'), strs[1].end());

                std::ostringstream response;
                response << "STATUSRESP," << MYGROUP << "," << strs[1];

                std::map<std::string, int> group_message_count;

                // Go through the message_buffer and count the messages for servers
                // who are valid
                for(unsigned long int i = 0; i < message_buffer.size(); i++) {
                    s_message tmp_msg = message_buffer[i];
                    if(strcmp(tmp_msg.receiver.c_str(), "UNKNOWN") != 0 && tmp_msg.unread) {
                        if ( group_message_count.find(tmp_msg.receiver) == group_message_count.end() ) {
                            group_message_count[tmp_msg.receiver] = 1;
                        } else {
                            group_message_count[tmp_msg.receiver] += 1;
                        }
                    }
                }

                // Go throug the collection and construct GROUP_ID,<NUM_OF_MESSAGES>
                // for the STATUSRESPONSE
                for (auto const& gmc : group_message_count) {
                    response << "," << gmc.first << "," << gmc.second;
                }

                std::string response_msg = constructMessage(response.str());
                response << " | SENT TO SERVER" << std::endl;
                writeToLog(response.str());
                send(serverSocket, response_msg.c_str(), response_msg.length(), 0);
            }
        } else if (strs[0] == "STATUSRESP") {
            // Not implemented yet
            std::cout << "Received STATUSRESP response" << std::endl;
        } else {
            std::string e_msg = "INVALID COMMAND!";
            writeToLog("Sent INVALID COMMAND back to server");
            send(serverSocket, e_msg.c_str(), e_msg.length()-1, 0);
        }
    }
    memset(&buffer, 0, sizeof(buffer));
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

    // Since we are calling the timedTasks right after timeout, we want to make sure that at least
    // a minute has passed
    std::chrono::system_clock::time_point clock_now = std::chrono::system_clock::now();
    std::time_t next_ping = std::chrono::system_clock::to_time_t(clock_now + std::chrono::seconds(60));
    std::time_t now = std::chrono::system_clock::to_time_t(clock_now);

    int maxfds;                     // Passed to select() as max fd in set
    struct sockaddr_in client;
    socklen_t clientLen;

    struct sockaddr_in server;
    socklen_t serverLen;
    char buffer[5000];              // buffer for reading from clients

    if(argc != 4) {
        //CLIENTS is mainly used to make the port distinction more 
        //readable for people searching for servers via the 'w' command
        printf("Usage: chat_server <server listen port> CLIENTS <client listen port>\n");
        exit(0);
    }

    // Setup sockets for server to listen to
    clistenSock = open_socket(atoi(argv[3]));
    printf("Listening for clients on port: %d\n", atoi(argv[3]));
    slistenSock = open_socket(atoi(argv[1]));
    printf("Listening for server on port: %d\n", atoi(argv[1]));

    if(listen(clistenSock, BACKLOG) < 0) {
        printf("Listening for clients failed on port %s\n", argv[3]);
        exit(0);
    } else
        // Add listen socket to socket set we are monitoring
    {
        FD_SET(clistenSock, &openSockets);
        maxfds = clistenSock;
    }

    if(listen(slistenSock, BACKLOG) < 0) {
        printf("Listening for servers failed on port %s\n", argv[1]);
        exit(0);
    } else {
        FD_SET(slistenSock, &openSockets);
        maxfds = slistenSock;
    }

    finished = false;

    while(!finished) {
        // This is re-initialized every loop because 
        // it seems to need a reset to work properly
        struct timeval timeout;      
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;        // Seems to need to be set. No harm done, not used

        // Get modifiable copy of readSockets
        readSockets = exceptSockets = openSockets;
        memset(buffer, 0, sizeof(buffer));

        // Look at sockets and see which ones have something to be read()
        int n = select(maxfds + 1, &readSockets, NULL, &exceptSockets, &timeout);

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
                ss << "Client connected to server on socket: " << clientSock << std::endl;
                writeToLog(ss.str());


            }

            // Does exactly the same thing as the FD_ISSET for clients above
            // but this is for servers
            if(FD_ISSET(slistenSock, &readSockets)) {
                serverLen = sizeof(server);
                serverSock = accept(slistenSock, (struct sockaddr *)&server,
                        &serverLen);

                // Add new servers to the list of open sockets
                FD_SET(serverSock, &openSockets);

                // And update the maximum file descriptor
                maxfds = std::max(maxfds, serverSock);

                // create a new server to store information.
                if(servers.size() < 5) {
                    servers[serverSock] = new Server(serverSock, inet_ntoa(server.sin_addr));
                }
                // Decrement the number of sockets waiting to be dealt with
                n--;

                printf("Server connected on server: %d\n", serverSock);
                std::ostringstream ss;
                ss << "Server connected to server on socket: " << serverSock << std::endl;
                writeToLog(ss.str());

                // Send our server information as soon as a connection is made
                std::string myIp = getOwnIp();
                std::ostringstream response_msg;
                response_msg << "SERVERS," << MYGROUP << "," << myIp << "," << argv[1] << ";";
                std::string response = constructMessage(response_msg.str());

                response_msg << " | SENT TO SERVER ALONG WITH A LISTSERVERS REQUEST" << std::endl;
                writeToLog(response_msg.str());
                send(serverSock, response.c_str(), response.length(), 0);

                std::ostringstream request;
                request << "LISTSERVERS," << MYGROUP;
                std::string crequest = constructMessage(request.str());
                send(serverSock, crequest.c_str(), crequest.length(), 0);
            }

            // Now check for commands from clients
            while(n > 0) {
                //Initialize a vector to collect servers
                //who have closed their connections
                //and close them outside of the iterator
                std::vector<int> serversToClose;

                // Go through the servers and check if they have issued commands
                for(auto const& pair : servers) {
                    Server *server = pair.second;

                    if(FD_ISSET(server->sock, &readSockets)) {
                        // recv() == 0 means client has closed connection
                        // We peek at the message, if it is valid then we recv for real 
                        // from (isMessageValid)
                        if(recv(server->sock, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT) == 0) {
                            printf("Server closed connection: %d", server->sock);
                            serversToClose.push_back(server->sock);
                        }
                        // We don't check for -1 (nothing received) because select()
                        // only triggers if there is something on the socket for us.
                        else {
                            if(isMessageValid((std::string) buffer)) {
                                if(recv(server->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0) {
                                    printf("Server closed connection: %d", server->sock);
                                    serversToClose.push_back(server->sock);
                                }
                                std::cout << buffer << std::endl;
                                serverCommand(server->sock, &openSockets, &maxfds,
                                        buffer, argv[1]);
                                memset(&buffer, 0, sizeof(buffer));
                            } else {
                                std::string checker = (std::string) buffer;
                                if(checker.length() > 0) {
                                    if((int) checker.at(0) != 0x01) {
                                        send(server->sock, "START MSG WITH 0x1 PLEASE", 25, 0);
                                    }
                                } else {
                                    send(server->sock, "NO EMPTY MSG", 12, 0);
                                }
                                memset(&buffer, 0, sizeof(buffer));
                            }
                        }
                    }
                }

                std::vector<int> clientsToClose;
                for(auto const& pair : clients) {
                    Client *client = pair.second;

                    if(FD_ISSET(client->sock, &readSockets)) {
                        // recv() == 0 means client has closed connection
                        if(recv(client->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0) {
                            printf("Client closed connection: %d", client->sock);
                            clientsToClose.push_back(client->sock);
                        }
                        // We don't check for -1 (nothing received) because select()
                        // only triggers if there is something on the socket for us.
                        else {
                            std::cout << buffer << std::endl;
                            int clientStatus = clientCommand(client->sock, &openSockets, &maxfds,
                                    buffer, argv[1]);

                            if(clientStatus == -1) {
                                clientsToClose.push_back(client->sock);
                            }
                        }
                    }
                }

                
                // If there are any servers or clients to close, then this
                // is the place to do it. Moving this into the iterator
                // will most likely result in a segfault
                for(long unsigned int i = 0; i < serversToClose.size(); i++) {
                    close(serversToClose[i]);
                    closeServer(serversToClose[i], &openSockets, &maxfds);
                }

                for(long unsigned int i = 0; i < clientsToClose.size(); i++) {
                    close(clientsToClose[i]);
                    closeClient(clientsToClose[i], &openSockets, &maxfds);
                }
                n--;
            }
        }

        // This checks whether it is time to do the timedTasks
        clock_now = std::chrono::system_clock::now();
        now = std::chrono::system_clock::to_time_t(clock_now);
        if(now > next_ping) {
            timedTasks();
            next_ping = std::chrono::system_clock::to_time_t(clock_now + std::chrono::seconds(60));
        }
    }
}
