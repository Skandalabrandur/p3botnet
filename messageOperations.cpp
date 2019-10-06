#include "messageOperations.h"
#include <iostream>
#include <stdio.h>

bool isMessageValid(std::string msg) {
    return ((int) msg.at(0) == 0x01 && (int) msg.at(msg.length() - 1) == 0x04);
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
//}
