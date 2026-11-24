#include "protocol.hpp"

namespace Protocol {
    bool isValidMessage(const std::string& message) {
        return !message.empty();
    }
}
