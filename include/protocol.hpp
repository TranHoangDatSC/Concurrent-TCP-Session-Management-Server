#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <string>

// Wire protocol validation and framing utilities for TCP sessions
namespace Protocol {
    // Returns true if 'message' conforms to the protocol syntax and length rules
    bool isValidMessage(const std::string& message);
}

#endif
