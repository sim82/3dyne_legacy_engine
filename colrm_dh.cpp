#include <iostream>
#include <string>
#include <iterator>

int main() {

    std::string str;

    while( !std::cin.eof() ) {
        std::getline( std::cin, str );
        if( str.length() > 2 ) {
            std::copy( str.begin() + 2, str.end(), std::ostream_iterator<char>(std::cout) );
            std::cout << '\n';
        }
        
    }

}