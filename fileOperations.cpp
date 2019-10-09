#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include "fileOperations.h"
using namespace std;

// Reference: http://www.cplusplus.com/doc/tutorial/files/
// Reference: https://en.cppreference.com/w/cpp/chrono

void writeToLog(std::string text) {
    //oh boy
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    ofstream logfile;
    logfile.open ("log.txt", ios::app);
    logfile << text << std::endl;
    logfile << " | " << std::ctime(&now) << std::endl;
    logfile.close();
}

//int main () {
//    writeToLog("This was written, when?");
//    return 0;
//}
