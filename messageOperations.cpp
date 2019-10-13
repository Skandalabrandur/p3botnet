#include "messageOperations.h"
#include <iostream>
#include <sstream>
#include <stdio.h>

// Checks whether a message has the correct 0x01 and 0x04 around it
bool isMessageValid(std::string msg) {
    return (msg.length() > 2 && (int) msg.at(0) == 0x01 && (int) msg.at(msg.length() - 1) == 0x04);
}

// Constructs a message with the correct 0x01 and 0x04 around it
std::string constructMessage(std::string msg) {
    std::stringstream ss;
    ss << (char) 0x01;
    ss << msg;
    ss << (char) 0x04;
    return ss.str();
}

// Strips 0x01 and 0x04 from a message
// Returns an empty string if 0x01 and 0x04 are not both present
std::string extractMessage(std::string msg) {
    if (isMessageValid(msg)) {
        return msg.substr(1, msg.length() - 2);
    } else {
        return "";
    }
}


//int main() {
//    char a[] = { 65, 66, 67 };
//    std::string invalid(a);
//
//    char b[] = { 0x01, 65, 66, 67, 0x04 };
//    std::string valid(b);
//
//    std::cout << invalid << std::endl;
//    std::cout << "isMessageValid(invalid) = " << isMessageValid(invalid) << std::endl;
//    std::cout << std::endl;
//    std::cout << valid << std::endl;
//    std::cout << "isMessageValid(valid) = " << isMessageValid(valid) << std::endl;

//    std::string newmsg;
//    std::string rawmsg;
//
//    rawmsg = "Hello fellow kiddos";
//    newmsg = constructMessage(rawmsg);
//
//    std::cout << "isMessageValid(rawmsg) = " << isMessageValid(rawmsg) << std::endl;
//    std::cout << "isMessageValid(newmsg) = " << isMessageValid(newmsg) << std::endl;
//}
