#ifndef UNIQUE_FD_HPP
#define UNIQUE_FD_HPP

#include <unistd.h>

class UniqueFd {
public:
    // Sentinel value for an invalid file descriptor
    static constexpr int kInvalidFd = -1;

private:
    int fd_ = kInvalidFd;

public:
    // Takes ownership of the file descriptor
    UniqueFd() noexcept = default;
    explicit UniqueFd(int fd) noexcept : fd_(fd) {}

    // Releases the owned file descriptor
    ~UniqueFd() noexcept {
        reset();
    }

    // Prevents multiple objects from owning the same descriptor
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    // Transfers ownership from another instance
    UniqueFd(UniqueFd&& other) noexcept
        : fd_(other.release()) {
    }

    // Releases the current descriptor and takes ownership of another
    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) reset(other.release());
        return *this;
    }

    // Returns the descriptor without transferring ownership
    int get() const noexcept {
        return fd_;
    }

    // Checks whether the descriptor is valid
    bool valid() const noexcept {
        return fd_ >= 0;
    }

    // Releases ownership without closing the descriptor
    int release() noexcept {
        const int fd = fd_;
        fd_ = kInvalidFd;
        return fd;
    }

    // Replaces the owned descriptor, closing the previous one
    void reset(int new_fd = kInvalidFd) noexcept {
        if (fd_ == new_fd) return;
        if (valid()) ::close(fd_);
        fd_ = new_fd;
    };
};
#endif
