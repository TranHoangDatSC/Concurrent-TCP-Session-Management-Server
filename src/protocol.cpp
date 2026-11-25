#include "protocol.hpp"

#include <algorithm>

namespace Protocol {
    namespace {
        // Strips trailing '\n' and '\r' line endings from a message
        std::string stripLineEnding(std::string s) {
            if (!s.empty() && s.back() == '\n') // Endline
                s.pop_back();
            if (!s.empty() && s.back() == '\r') // Escape sequence
                s.pop_back();
            return s;
        }

        // Splits a string into fields delimited by '|', preserving empty tokens
        std::vector<std::string> splitByPipe(const std::string& s) {
            std::vector<std::string> parts;
            std::size_t start = 0;

            while (true) {
                std::size_t pos = s.find('|', start);
                if (pos == std::string::npos) {
                    parts.push_back(s.substr(start));
                    break;
                }
                parts.push_back(s.substr(start, pos - start));
                start = pos + 1;
            }
            return parts;
        }
    } // namespace

    bool isValidMessage(const std::string& message) {
        return !message.empty();
    }
} // namespace
