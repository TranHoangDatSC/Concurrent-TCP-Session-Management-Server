#include "protocol.hpp"

namespace Protocol {

    // 1. Extract complete frames from TCP receive buffer
    FrameResult extractFrames(std::string& buffer) {
        FrameResult result;

        std::size_t newline_position;

        // Keep extracting while a newline exists
        while ((newline_position = buffer.find('\n')) != std::string::npos) {

            // Check the payload size before extracting
            if (newline_position > MAX_MESSAGE_SIZE) {
                result.too_large = true;
                buffer.clear();
                return result;
            }

            // Extract everything before '\n'
            result.frames.push_back(
                buffer.substr(0, newline_position)
            );

            // Remove the extracted frame and its '\n'
            buffer.erase(0, newline_position + 1);
        }

        // No complete frame remains
        // Check whether the incomplete buffer is already too large
        if (buffer.size() > MAX_MESSAGE_SIZE) {
            result.too_large = true;
            buffer.clear();
        }

        return result;
    }

    // 2. Parse a client request
    std::optional<Request> parseRequest(const std::string& line) {
        // Reject empty or oversized messages
        if (line.empty() || line.size() > MAX_MESSAGE_SIZE)
            return std::nullopt;

        // A frame must not contain another newline.
        if (line.find('\n') != std::string::npos)
            return std::nullopt;

        Request request;

        // Find the first separator
        std::size_t separator_position = line.find('|');

        // No separator means the command has no parameters.
        if (separator_position == std::string::npos) {
            request.command = line;
            return request;
        }

        // Extract the command
        request.command = line.substr(0, separator_position);

        if (request.command.empty()) {
            return std::nullopt;
        }

        // Extract everything after the first '|'
        request.raw_parameters =
            line.substr(separator_position + 1);

        // Split parameters while preserving empty tokens
        std::size_t start = 0;

        while (true) {
            std::size_t next_separator =
                request.raw_parameters.find('|', start);

            if (next_separator == std::string::npos) {
                request.params.push_back(
                    request.raw_parameters.substr(start)
                );
                break;
            }

            request.params.push_back(
                request.raw_parameters.substr(
                    start,
                    next_separator - start
                )
            );

            start = next_separator + 1;
        }

        return request;
    }

    // 3. Parse a server response
    std::optional<Response> parseResponse(const std::string& line) {
        // Check for validity
        if (line.empty() || line.size() > MAX_MESSAGE_SIZE)
            return std::nullopt;

        if (line.find('\n') != std::string::npos)
            return std::nullopt;

        // Find the first separators
        std::size_t first_separator = line.find('|');

        if (first_separator == std::string::npos)
            return std::nullopt;

        // Extract the response type
        std::string status = line.substr(0, first_separator);

        // Find the second separators
        std::size_t second_separator =
            line.find('|', first_separator + 1);

        // Define the code and the payload
        std::string code;
        std::string payload;

        if (second_separator == std::string::npos)
            code = line.substr(first_separator + 1);
        else {
            // Ex payload: OK|SESSION_CREATE|1001
            code = line.substr(
                first_separator + 1,
                second_separator - first_separator - 1
            );
            payload = line.substr(second_separator + 1);
        }

        // Only OK and ERROR are accepted
        if (status != "OK" && status != "ERROR")
            return std::nullopt;

        // The response code must not be empty
        if (code.empty())
            return std::nullopt;

        // Error reponses must contain a description field
        if (status == "ERROR" && second_separator == std::string::npos)
            return std::nullopt;

        Response response;
        response.ok = (status == "OK");
        response.code = code;
        response.payload = payload;

        return response;
    }

    // 4. Format a successful response
    std::string formatOK(
        const std::string& code,
        const std::string& data
    ) {
        if (data.empty()) return "OK|" + code + "\n";
        return "OK|" + code + "|" + data + "\n";
    }

    // 5. Format an error response
    std::string formatError(
        const std::string& error_code,
        const std::string& description
    ) {
        return "ERROR|" + error_code + "|" + description + "\n";
    }

} // namespace Protocol
