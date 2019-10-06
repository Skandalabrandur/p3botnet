#include <iostream>
#include <boost/circular_buffer.hpp>

#define START 0X01
#define END 0x04

typedef boost::circular_buffer<char> circular_buffer;
circular_buffer cb{600};



int main(){
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