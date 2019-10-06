#ifndef DATA_OBJ_H
#define DATA_OBJ_H
#include <string.h>

struct s_message {
    std::string msg;        // The message contents
    std::string sender;     // GROUP_ID, who sent the message?
    std::string receiver;   // GROUP_ID, who shall receive?
};

struct s_server {
    int fd;
    std::string group_id;
};

#endif
