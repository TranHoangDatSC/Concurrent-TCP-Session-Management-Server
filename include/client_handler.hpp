#pragma once

#include <cstddef>
#include <string>
#include "unique_fd.hpp"

class ClientHandler {
private:
    UniqueFd client_fd_;
    std::string recv_buffer_;

    void sendAll(const std::string& data);
    void handleLine(const std::string& line);

public:
    explicit ClientHandler(UniqueFd client_fd) noexcept;
    ~ClientHandler() noexcept = default;

    ClientHandler(const ClientHandler&) = delete;
    ClientHandler& operator=(const ClientHandler&) = delete;

    ClientHandler(ClientHandler&&) noexcept = default;
    ClientHandler& operator=(ClientHandler&&) noexcept = default;

    void run();
    int getClientFd() const noexcept;
};
