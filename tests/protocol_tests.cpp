#include "protocol.hpp"

#include <iostream>
#include <string>
#include <vector>

int passed = 0;
int failed = 0;

// Prints the result of one test case
void check(bool condition, const std::string& test_name) {
    if (condition) {
        std::cout << "[PASS] " << test_name << '\n';
        ++passed;
    }
    else {
        std::cout << "[FAIL] " << test_name << '\n';
        ++failed;
    }
}

// 1. Frame extraction tests
void testExtractFrames() {
    // Test 1: Extract one complete frame
    {
        std::string buffer = "PING\n";

        Protocol::FrameResult result =
            Protocol::extractFrames(buffer);

        check(
            result.frames.size() == 1 &&
            result.frames[0] == "PING" &&
            buffer.empty() &&
            !result.too_large,
            "Extract one complete frame"
        );
    }

    // Test 2: Extract multiple frames from one buffer
    {
        std::string buffer = "PING\nLOGIN|alice|123\n";

        Protocol::FrameResult result =
            Protocol::extractFrames(buffer);

        check(
            result.frames.size() == 2 &&
            result.frames[0] == "PING" &&
            result.frames[1] == "LOGIN|alice|123" &&
            buffer.empty() &&
            !result.too_large,
            "Extract multiple frames"
        );
    }

    // Test 3: Keep incomplete data in the buffer
    {
        std::string buffer = "PING";

        Protocol::FrameResult result =
            Protocol::extractFrames(buffer);

        check(
            result.frames.empty() &&
            buffer == "PING" &&
            !result.too_large,
            "Keep incomplete frame"
        );
    }

    // Test 4: Extract complete frames and preserve incomplete data
    {
        std::string buffer = "PING\nLOGIN|alice";

        Protocol::FrameResult result =
            Protocol::extractFrames(buffer);

        check(
            result.frames.size() == 1 &&
            result.frames[0] == "PING" &&
            buffer == "LOGIN|alice" &&
            !result.too_large,
            "Extract complete frame and keep remaining data"
        );
    }

    // Test 5: Accept a frame with exactly 1024 bytes
    {
        std::string buffer(MAX_MESSAGE_SIZE, 'A');
        buffer += '\n';

        Protocol::FrameResult result =
            Protocol::extractFrames(buffer);

        check(
            result.frames.size() == 1 &&
            result.frames[0].size() == MAX_MESSAGE_SIZE &&
            !result.too_large,
            "Accept frame with exactly 1024 bytes"
        );
    }

    // Test 6: Reject a complete frame larger than 1024 bytes
    {
        std::string buffer(MAX_MESSAGE_SIZE + 1, 'A');
        buffer += '\n';

        Protocol::FrameResult result =
            Protocol::extractFrames(buffer);

        check(
            result.too_large &&
            buffer.empty(),
            "Reject oversized complete frame"
        );
    }

    // Test 7: Reject an incomplete buffer larger than 1024 bytes
    {
        std::string buffer(MAX_MESSAGE_SIZE + 1, 'A');

        Protocol::FrameResult result =
            Protocol::extractFrames(buffer);

        check(
            result.too_large &&
            buffer.empty(),
            "Reject oversized incomplete frame"
        );
    }
}

// 2. Request parsing tests
void testParseRequest() {
    // Test 1: Parse a command without parameters
    {
        auto request = Protocol::parseRequest("PING");

        check(
            request.has_value() &&
            request->command == "PING" &&
            request->raw_parameters.empty() &&
            request->params.empty(),
            "Parse request without parameters"
        );
    }

    // Test 2: Parse a command with multiple parameters
    {
        auto request =
            Protocol::parseRequest("LOGIN|alice|123456");

        check(
            request.has_value() &&
            request->command == "LOGIN" &&
            request->raw_parameters == "alice|123456" &&
            request->params.size() == 2 &&
            request->params[0] == "alice" &&
            request->params[1] == "123456",
            "Parse request with multiple parameters"
        );
    }

    // Test 3: Preserve an empty parameter
    {
        auto request =
            Protocol::parseRequest("COMMAND|alice|");

        check(
            request.has_value() &&
            request->params.size() == 2 &&
            request->params[0] == "alice" &&
            request->params[1].empty(),
            "Preserve empty parameter"
        );
    }

    // Test 4: Reject an empty request
    {
        auto request = Protocol::parseRequest("");

        check(
            !request.has_value(),
            "Reject empty request"
        );
    }

    // Test 5: Reject a request containing LF
    {
        auto request =
            Protocol::parseRequest("PING\nLOGIN");

        check(
            !request.has_value(),
            "Reject request containing newline"
        );
    }

    // Test 6: Reject a request larger than 1024 bytes
    {
        std::string line(MAX_MESSAGE_SIZE + 1, 'A');

        auto request = Protocol::parseRequest(line);

        check(
            !request.has_value(),
            "Reject oversized request"
        );
    }
}

// 3. Response parsing tests
void testParseResponse() {
    // Test 1: Parse a successful response
    {
        auto response =
            Protocol::parseResponse("OK|200|LOGIN_SUCCESS");

        check(
            response.has_value() &&
            response->ok &&
            response->code == "200" &&
            response->payload == "LOGIN_SUCCESS",
            "Parse successful response"
        );
    }

    // Test 2: Parse an error response
    {
        auto response =
            Protocol::parseResponse(
                "ERROR|INVALID_ARGS|Missing username"
            );

        check(
            response.has_value() &&
            !response->ok &&
            response->code == "INVALID_ARGS" &&
            response->payload == "Missing username",
            "Parse error response"
        );
    }

    // Test 3: Preserve additional separators in the payload
    {
        auto response =
            Protocol::parseResponse("OK|200|alice|online");

        check(
            response.has_value() &&
            response->payload == "alice|online",
            "Preserve separators inside response payload"
        );
    }

    // Test 4: Accept an empty payload
    {
        auto response =
            Protocol::parseResponse("OK|200|");

        check(
            response.has_value() &&
            response->ok &&
            response->payload.empty(),
            "Parse response with empty payload"
        );
    }

    // Test 5: Reject an unknown response type
    {
        auto response =
            Protocol::parseResponse("UNKNOWN|200|DATA");

        check(
            !response.has_value(),
            "Reject unknown response type"
        );
    }

    // Test 6: Reject a response without a code
    {
        auto response =
            Protocol::parseResponse("OK||DATA");

        check(
            !response.has_value(),
            "Reject response with empty code"
        );
    }

    // Test 7: Reject a response without enough separators
    {
        auto response =
            Protocol::parseResponse("OK|200");

        check(
            !response.has_value(),
            "Reject malformed response"
        );
    }
}

// 4. Response formatting tests
void testFormatResponse() {
    // Test 1: Format a successful response
    {
        std::string response =
            Protocol::formatOK("200", "LOGIN_SUCCESS");

        check(
            response == "OK|200|LOGIN_SUCCESS\n",
            "Format successful response"
        );
    }

    // Test 2: Format an error response
    {
        std::string response =
            Protocol::formatError(
                "INVALID_ARGS",
                "Missing username"
            );

        check(
            response ==
            "ERROR|INVALID_ARGS|Missing username\n",
            "Format error response"
        );
    }

    // Test 3: Format a response with an empty payload
    {
        std::string response =
            Protocol::formatOK("200");

        check(
            response == "OK|200|\n",
            "Format response with empty payload"
        );
    }
}

// Main
int main() {
    std::cout << "===== CTSMS Protocol Tests =====\n\n";

    testExtractFrames();
    testParseRequest();
    testParseResponse();
    testFormatResponse();

    std::cout << "\n===== Test Summary =====\n";
    std::cout << "Passed: " << passed << '\n';
    std::cout << "Failed: " << failed << '\n';

    if (failed == 0) {
        std::cout << "\nAll tests passed!\n";
        return 0;
    }

    std::cout << "\nSome tests failed.\n";
    return 1;
}
