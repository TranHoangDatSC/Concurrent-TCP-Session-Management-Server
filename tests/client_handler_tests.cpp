#include "client_handler.hpp"
#include "protocol.hpp"
#include "unique_fd.hpp"
#include "terminal_ui.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <string>
#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include <cerrno>

int passed = 0;
int failed = 0;

// Print the result of one test case
void check(bool condition, const std::string& test_name) {
    if (condition) {
        TerminalUI::print_success(test_name);
        ++passed;
    }
    else {
        TerminalUI::print_error(test_name);
        --failed;
    }
}

// Helper simulating a fake TCP link between test code and ClientHandler
struct TestConnection {
    UniqueFd client_fd;
    std::thread worker; // Background thread running ClientHandler::run()

    bool start() {
        // Two connected socket FDs:
        int sockets[2]; // [0] for handler, [1] for test

        // Create an in-memory connected stream socket pair
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0l, sockets) < 0)
            return false;

        // Set 2 sec read timeout, help tests never hang forever
        timeval timeout{};
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;

        if (::setsockopt(
            sockets[1],
            SOL_SOCKET,
            SO_RCVTIMEO,
            &timeout,
            sizeof(timeout)
        ) < 0) {
            ::close(sockets[0]);
            ::close(sockets[1]);
            return false;
        }

        // Test owns sockets[1]
        client_fd.reset(sockets[1]);

        // Move sockets[0] into ClientHandler
        ClientHandler handler{ UniqueFd(sockets[0]) };

        // Run handler on a separate thread so test can send/recv concurrectly
        worker = std::thread(
            [handler = std::move(handler)]() mutable {
                handler.run();
            }
        );
        return true;
    }

    // Checking are these data can be send sucessfully
    bool sendData(const std::string& data) {
        std::size_t sent = 0;

        while (sent < data.size()) {
            ssize_t n = ::send(
                client_fd.get(),
                data.data() + sent,
                data.size() - sent,
                MSG_NOSIGNAL
            );

            if (n > 0) sent += static_cast<std::size_t>(n);
            else if (n < 0 && errno == EINTR) continue;
            else return false;
        } return true;
    }

    // Receive until the end of the line
    std::string receiveLine() {
        std::string response;
        char ch;

        while (true) {
            ssize_t n = ::recv(client_fd.get(), &ch, 1, 0);

            if (n == 1) {
                if (ch == '\n') break;
                response += ch;
            }
            else break;
        } return response;
    }

    void stop() {
        // Tell the handler that the client will not send more data
        if (client_fd.valid())
            ::shutdown(client_fd.get(), SHUT_WR);
        if (worker.joinable()) worker.join();
        client_fd.reset();
    }

    ~TestConnection() {
        stop();
    }
};

// Test 1: PING should receive PONG
void testPing() {
    TestConnection connection;

    if (!connection.start()) {
        check(
            false,
            "Start ClientHandler for PING"
        ); return;
    }
    const bool sent = connection.sendData("PING\n");
    const std::string response = connection.receiveLine();

    check(send, "Send PING request");
    check(response == "OK|PONG", "PING returns OK|PONG");

    connection.stop();
}

// Test 2: QUIT should respond and close the connection
void testQuit() {
    TestConnection connection;

    if (!connection.start()) {
        check(false, "Start ClientHandler for QUIT");
        return;
    }

    const bool sent = connection.sendData("QUIT\n");
    const std::string response = connection.receiveLine();

    check(sent, "Send QUIT request");
    check(response == "OK|BYE", "QUIT returns OK|BYE");

    // The handler should close its socket after processing QUIT
    char ch;

    const ssize_t n = ::recv(connection.client_fd.get(), &ch, 1, 0);

    check(n == 0, "QUIT closes the client connection");

    connection.stop();
}

// Test 3: Unknown commands should be rejected
void testInvalidCommand() {
    TestConnection connection;

    if (!connection.start()) {
        check(false, "Start ClientHandler for invalid command");
        return;
    }

    const bool sent = connection.sendData("UNKNOWN\n");
    const std::string response = connection.receiveLine();

    check(sent, "Send unknown command");

    check(
        response == "ERROR|INVALID_COMMAND|Unsupported command",
        "Reject unknow command"
    );

    connection.stop();
}

// Test 4: Incorrect parameter count should be rejected
void testInvalidParameters() {
    TestConnection connection;

    if (!connection.start()) {
        check(false, "Start ClientHandler for invalid parameters");
        return;
    }

    const bool sent = connection.sendData("LOGIN|alice\n");
    const std::string response = connection.receiveLine();

    check(sent, "Send LOGIN with missing parameter");

    check(
        response == "ERROR|INVALID_PARAMETERS|Incorrect number of parameters",
        "Reject incorrect parameter count"
    );

    connection.stop();
}

// Test 5: Multiple requests in one TCP write should be processed separately
void testMultipleRequests() {
    TestConnection connection;

    if (!connection.start()) {
        check(false, "Start ClientHandler for multiple requests");
        return;
    }

    const bool sent = connection.sendData("PING\nPING\n");

    const std::string response1 = connection.receiveLine();
    const std::string response2 = connection.receiveLine();

    check(sent, "Send multiple requests in one write");
    check(response1 == "OK|PONG", "Receive first PONG");
    check(response2 == "OK|PONG", "Receive second PONG");

    connection.stop();
}

// Test 6: A request split across multiple writes should still be parsed
void testFragmentedRequest() {
    TestConnection connection;

    if (!connection.start()) {
        check(false, "Start ClientHandler for fragmented request");
        return;
    }

    const bool sent_first = connection.sendData("PI");
    const bool sent_second = connection.sendData("NG\n");

    const std::string response = connection.receiveLine();

    check(sent_first && sent_second, "Send fragmented PING request");
    check(response == "OK|PONG", "Process fragmented PING correctly");

    connection.stop();
}

int main() {
    TerminalUI::print_system("Running ClientHandler tests...");

    testPing();
    testQuit();
    testInvalidCommand();
    testInvalidParameters();
    testMultipleRequests();
    testFragmentedRequest();

    TerminalUI::print_system(
        "ClientHandler tests completed. Passed: " +
        std::to_string(passed) +
        ", Failed: " +
        std::to_string(failed)
    );

    return failed == 0 ? 0 : 1;
}
