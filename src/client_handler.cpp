#include "client_handler.hpp"
#include "protocol.hpp"
#include "terminal_ui.hpp"

#include <sys/socket.h> // POSIX socket I/O (recv, send)
#include <unistd.h>     // POSIX system definitions
#include <cerrno>       // Error codes
#include <utility>      // std::move for ownership transfer
#include <unordered_map>

// Take sole ownership of client socket
ClientHandler::ClientHandler(UniqueFd client_fd) noexcept
    : client_fd_(std::move(client_fd)) {
}

int ClientHandler::getClientFd() const noexcept {
    return client_fd_.get(); // For temporary use (without transfer of ownership)
}

void ClientHandler::run() {
    // Exit early if socket is invalid
    if (!client_fd_.valid()) return;

    // Temporary read buffer for one recv() call
    char temp[1024];

    while (true) {
        // Block waiting for incoming bytes from the client
        ssize_t n = ::recv(client_fd_.get(), temp, sizeof(temp), 0);

        if (n > 0) {
            // Append received bytes to persistent stream buffer
            recv_buffer_.append(temp, static_cast<std::size_t>(n));

            // Split stream buffer into complete '\n'-delimited frames
            auto framed = Protocol::extractFrames(recv_buffer_);

            // Disconnect if a single frame exceeds payload limit
            if (framed.too_large) {
                const std::string err = Protocol::formatError(
                    "ERR_FRAME_TOO_LARGE",
                    "Frame exceeds max payload size"
                );
                if (!sendAll(err)) {
                    TerminalUI::print_warning(
                        "Failed to send frame-too-large error to client fd!"
                        + std::to_string(client_fd_.get())
                    );
                } break;
            }

            bool keep_running = true;
            for (const auto& line : framed.frames) {
                if (!handleLine(line)) {
                    keep_running = false;
                    break;
                }
            } if (!keep_running) break;
        }

        // Client closed the connection gracefully
        else if (n == 0) break;

        // Retry if interrupted by a signal, otherwise exit loop
        else {
            if (errno == EINTR) continue;
            break;
        }
    }

    // Explicitly release/close socket before thread finishes
    client_fd_.reset();
}

bool ClientHandler::sendAll(const std::string& data) {
    if (!client_fd_.valid()) return false;

    // Send data until buffer full
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t n = ::send(
            client_fd_.get(),
            data.data() + sent,
            data.size() - sent,
            MSG_NOSIGNAL
        );
        if (n > 0) sent += static_cast<std::size_t>(n);

        else if (n < 0 && errno == EINTR) continue;

        else return false;
    } return true;
}

bool ClientHandler::handleLine(const std::string& line) {
    auto request = Protocol::parseRequest(line);

    // Validate request syntax
    if (!request.has_value()) {
        return sendAll(Protocol::formatError(
            "INVALID_REQUEST",
            "Malformed request"
        ));
    }

    // Define supported commands and their expected parameter counts
    static const std::unordered_map<std::string, std::size_t>
        expected_params = {
            { "LOGIN", 2 },
            { "LOGOUT", 0 },
            { "LIST_USERS", 0 },
            { "CREATE_SESSION", 1 },
            { "ACCEPT_SESSION", 1 },
            { "REJECT_SESSION", 1 },
            { "END_SESSION", 1 },
            { "LIST_SESSIONS", 0 },
            { "PING", 0 },
            { "QUIT", 0 }
    };

    // Validate command
    auto it = expected_params.find(request->command);
    if (it == expected_params.end()) {
        return sendAll(Protocol::formatError(
            "INVALID_COMMAND",
            "Unsupported command"
        ));
    }

    // Validate parameter count
    if (request->params.size() != it->second) {
        return sendAll(Protocol::formatError(
            "INVALID_PARAMETERS",
            "Incorrect number of parameters"
        ));
    }

    // Handle PING command
    if (request->command == "PING")
        return sendAll(Protocol::formatOK("PONG"));

    // Handle QUIT command
    if (request->command == "QUIT") {
        if (!sendAll(Protocol::formatOK("BYE")))
            return false;
        return false; // Close this client connection
    }

    // Valid commands are not implement yet
    return sendAll(Protocol::formatError(
        "NOT_IMPLEMENTED",
        "Command is not implemented yet"
    ));
}
