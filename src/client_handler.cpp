#include "client_handler.hpp"
#include "protocol.hpp"

#include <sys/socket.h> // POSIX socket I/O (recv, send)
#include <unistd.h>     // POSIX system definitions
#include <cerrno>       // Error codes
#include <utility>      // std::move for ownership transfer

// Take sole ownership of client socket
ClientHandler::ClientHandler(UniqueFd client_fd) noexcept
    : client_fd_(std::move(client_fd)) {
}

// UniqueFd closes socket automatically
ClientHandler::~ClientHandler() noexcept {}

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

            // Disconnect if a single freem exceeds payload limit
            if (framed.too_large) {
                const std::string err = Protocol::formatError(
                    "ERR_FRAME_TOO_LARGE",
                    "Frame exceeds max payload size");
                sendAll(err);
                break;
            } for (const auto& line : framed.frames) handleLine(line);
        }
        // Client closed connection cleanly
        else if (n == 0) break;

        // Retry if interrupted by a signal, otherwise exit loop
        else {
            if (errno == EINTR) continue;
            break;
        }
    }

    // Explicitlu release/close socket before thread finishes
    client_fd_.reset();
}
