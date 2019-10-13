#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include "fileOperations.h"
using namespace std;

// Reference: http://www.cplusplus.com/doc/tutorial/files/
// Reference: https://en.cppreference.com/w/cpp/chrono

void writeToLog(std::string text) {
    // Get timestamp for now
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    // Append the message with a timestamp to log.txt
    ofstream logfile;
    logfile.open ("log.txt", ios::app);
    logfile << text;
    logfile << " | " << std::ctime(&now) << std::endl;
    logfile.close();
}

//int main () {
//    writeToLog("This was written, when?");
//    return 0;
//}
