#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <string>
#include <optional>
#include <vector>

// Wire protocol validation, framing, and serialization utilities for TCP sessions
namespace Protocol {
    struct Request {
        std::string command;             // Protocol command name.
        std::string raw_parameters;      // Unparsed parameter substring after '|'.
        std::vector<std::string> params; // Split parameter tokens.
    };

    struct Response {
        bool ok = false;   // True for OK responses, false for ERROR responses.
        std::string code;      // Status or result code, e.g., "200" or "INVALID_ARGS".
        std::string payload;   // Response DATA when ok is true; DESCRIPTION when false.
    };

    // Returns true if 'message' conforms to basic protocol framing and syntax rules.
    bool isValidMessage(const std::string& message);

    // Extracts complete '\n'-terminated frames from 'buffer', keeping incomplete data.
    std::vector<std::string> extractFrames(std::string& buffer);

    // Parses a single line formatted as COMMAND|PARAMETERS.
    std::optional<Request> parseRequest(const std::string& line);

    // Parses a single line formatted as OK|CODE|DATA or ERROR|ERROR_CODE|DESCRIPTION.
    std::optional<Response> parseResponse(const std::string& line);

    // Formats a success response terminated with '\n'.
    std::string formatOK(const std::string& code, const std::string& data = "");

    // Formats an error response terminated with '\n'.
    std::string formatError(const std::string& error_code, const std::string& description);

} // namespace Protocol

#endif
