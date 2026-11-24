#include "protocol.hpp"

#include <cassert> // Provides assert() for checking test conditions
#include <iostream>

int main() {

    // Verify that non-empty message is considered valid
    assert(Protocol::isValidMessage("PING"));

    // Verify that an empty message is considered invalid
    assert(!Protocol::isValidMessage(""));

    std::cout << "All protocol tests passed!\n";

    return 0;
}
