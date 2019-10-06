#ifndef DATA_OBJ_H
#define DATA_OBJ_H
#include <string.h>

// Server(int socket) - socket to send/receive traffic from client.
class Server 
{
public:
    int sock;              // socket of client connection
    std::string group_id;

    Server(int socket) : sock(socket) {}

    ~Server() {}           // Virtual destructor defined for base class
};


struct s_message {
    std::string msg;        // The message contents
    std::string sender;     // GROUP_ID, who sent the message?
    std::string receiver;   // GROUP_ID, who shall receive?
};

#endif
