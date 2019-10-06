#include <iostream>
#include <boost/circular_buffer.hpp>

typedef boost::circular_buffer<char> circular_buffer;
circular_buffer cb{500};
int main(){
    for(int i = 0; i < 500; i++){
        cb.push_back('a');
    }
    for(char i : cb){
        std::cout << i;
    }
    std::cout<< std::endl;
    for(int i = 0; i < 50; i++){
        cb.push_back('b');
    }
    for(char i : cb){
        std::cout << i;
    }
    return 0;
}