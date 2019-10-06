#include <iostream>
#include <boost/circular_buffer.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <vector>
#define START 0X01
#define END 0x04

typedef boost::circular_buffer<char> circular_buffer;
circular_buffer cb{600};

void parseByComma(){
    std::string msg[10];
    for(int i = 0; i < 10; i++){
        msg[i] = "Henlo";
    }
    std::string output = boost::algorithm::join(msg, ",");
    std::cout << output <<std::endl;
    std::vector<std::string> splitted;
    boost::algorithm::split(splitted, output, boost::algorithm::is_any_of(","));
    for(unsigned int i = 0; i < splitted.size(); i++){
        std::cout << splitted[i] << std::endl;
    }
}

int main(){
    parseByComma();
    cb.push_back(START);
    for(int i = 0; i < 500; i++){
        cb.push_back('a');
    }
    cb.push_back(END);
    for(char i : cb){
        std::cout << i;
    }
    cb.push_back(START);
    std::cout<< std::endl;
    for(int i = 0; i < 50; i++){
        cb.push_back('b');
    }
    cb.push_back(END);
    for(char i : cb){
        std::cout << i;
    }
    return 0;
}
