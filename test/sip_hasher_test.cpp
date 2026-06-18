#include <stl_extension/sip_hasher.h>
#include <iostream>
#include <iomanip>
#include <cstring>

using namespace sip_hasher;

void printHash(uint64_t hash)
{
    std::cout << std::hex << std::setfill('0') << std::setw(16) << hash << std::dec << std::endl;
}
 
int main() {
    // Example 1: One-shot hashing
    {
        std::array<uint8_t, 16> key{
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
        };
        
        const char* message = "Hello, World!";
        const char * data = message;
        size_t len = strlen(message);
 
        uint64_t hash = SipHasher::hash(key, data, len);
        
        std::cout << "Message: " << message << std::endl;
        std::cout << "Hash: ";
        printHash(hash);
    }
 
    {
        std::array<uint8_t, 16> key{
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
        };
        
        const char* message = "Hello, World!";
        const char * data = message;
        size_t len = strlen(message);
 
        uint64_t hash = SipHasher::hash(key, data, len);
        
        std::cout << "Message: " << message << std::endl;
        std::cout << "Hash: ";
        printHash(hash);
    }
 
    std::cout << std::endl;

    // Example 3: Hash table collision test
    {
        std::array<uint8_t, 16> key = {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
        };
 
        std::cout << "Hash distribution test:" << std::endl;
        for (int i = 0; i < 5; ++i) {
            std::string msg = "test_" + std::to_string(i);
            uint64_t hash = SipHasher::hash(key, (const char*)msg.c_str(), msg.length());
            std::cout << "  " << msg << " -> ";
            printHash(hash);
        }
    }
 
    return 0;
}