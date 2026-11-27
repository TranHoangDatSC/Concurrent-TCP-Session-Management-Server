# CTSMS — Interface Contract

**Version:** 1.0
**Language:** C++17
**Platform:** Linux / POSIX
**Project:** Concurrent TCP Session Management Server

---

## 1. Purpose

This document defines the responsibilities, interfaces, ownership rules, and interaction boundaries between CTSMS components.

The purpose is to:
- Establish clear responsibilities for each module.
- Prevent duplicated resource ownership.
- Define consistent interfaces before implementation.
- Reduce integration issues between components.
- Maintain a modular and testable architecture.

## 2. Component Overview

| Component | Responsibility |
|---|---|
| TCPServer | Creates, binds, listens on, and accepts TCP connections. |
| ClientHandler | Handles one connected client and its communication lifecycle. |
| TCPClient | Provides the client-side TCP connection and communication interface. |
| UniqueFd | Provides RAII-based file descriptor ownership. |
| Protocol | Handles message framing, parsing, validation, and serialization. |
| UserManager | Handles authentication and online user management. |
| SessionManager | Manages logical communication sessions. |
| TerminalUI | Provides synchronized terminal output. |

## 3. Resource Ownership Rules

### 3.1 File Descriptor Ownership

The project follows strict RAII-based ownership.

| Resource | Owner | Ownership Type |
|---|---|---|
| Listening socket | TCPServer | Exclusive |
| Accepted client socket | ClientHandler | Exclusive |
| Outgoing client socket | TCPClient | Exclusive |
| Active client FD registry | TCPServer | Non-owning |
| Session metadata | SessionManager | Exclusive logical ownership |
| User records | UserManager | Exclusive logical ownership |

Rules:
1. Every owned file descriptor must be managed by `UniqueFd`.
2. Copy operations must not duplicate ownership.
3. Ownership transfer must use move semantics.
4. `get()` provides non-owning access to the descriptor.
5. `release()` transfers ownership without closing the descriptor.
6. `reset()` replaces or releases the currently owned descriptor.
7. Non-owning references must never close the underlying descriptor.

### 3.2 Thread Ownership

- TCPServer owns the lifecycle of worker threads.
- Each ClientHandler is associated with one client connection.
- Worker threads must remain joinable.
- Detached threads are not permitted.
- Shared manager data must be protected by mutexes.
- Blocking socket operations must not execute while holding manager mutexes.

## 4. Module Interfaces

### 4.1 UniqueFd

**Header:** `include/unique_fd.hpp`

**Responsibility:** Exclusive ownership and automatic cleanup of a POSIX file descriptor.

Expected public interface:

```cpp
class UniqueFd {
public:
    static constexpr int kInvalidFd = -1;

    UniqueFd() noexcept;
    explicit UniqueFd(int fd) noexcept;
    ~UniqueFd() noexcept;

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept;
    UniqueFd& operator=(UniqueFd&& other) noexcept;

    int get() const noexcept;
    bool valid() const noexcept;
    int release() noexcept;
    void reset(int new_fd = kInvalidFd) noexcept;
};
```

**Ownership contract:**
- Takes ownership of a supplied descriptor.
- Closes the owned descriptor during destruction.
- Transfers ownership through move operations.
- Never allows accidental copying.

### 4.2 Protocol

**Header:** `include/protocol.hpp`
**Implementation:** `src/protocol.cpp`

**Responsibility:** Handle application-level protocol processing independently of socket operations.

Expected public interface:

```cpp
namespace Protocol {

struct Request {
    std::string command;
    std::string raw_parameters;
    std::vector<std::string> params;
};

struct Response {
    bool ok = false;
    std::string code;
    std::string payload;
};

struct FrameResult {
    std::vector<std::string> frames;
    bool too_large = false;
};

FrameResult extractFrames(std::string& buffer);

std::optional<Request> parseRequest(
    const std::string& line
);

std::optional<Response> parseResponse(
    const std::string& line
);

std::string formatOK(
    const std::string& code,
    const std::string& data = ""
);

std::string formatError(
    const std::string& error_code,
    const std::string& description
);

}
```

**Contract:**
- TCP framing uses LF (`\n`) as the delimiter.
- Maximum message payload is 1024 bytes, excluding LF.
- Incomplete frames must remain buffered.
- Multiple complete frames may be extracted from one buffer.
- Parsing validates syntax, not command-specific business rules.
- A frame exceeding the size limit must be treated as a connection-level protocol error.

### 4.3 TCPServer

**Header:** `include/tcp_server.hpp`
**Implementation:** `src/tcp_server.cpp`

**Responsibility:** Manage the listening socket and the lifecycle of client connections.

Expected public interface:

```cpp
class TCPServer {
public:
    TCPServer();
    ~TCPServer();

    TCPServer(const TCPServer&) = delete;
    TCPServer& operator=(const TCPServer&) = delete;

    bool start(
        const std::string& address,
        std::uint16_t port
    );

    void stop();

    bool isRunning() const;
};
```

**Private responsibilities:**
- Create and configure the listening socket.
- Bind to the configured address and port.
- Start listening for incoming connections.
- Accept client connections.
- Create and manage ClientHandler instances.
- Maintain a non-owning registry of active client descriptors.
- Coordinate graceful shutdown.
- Join all worker threads.

**Ownership contract:**
- TCPServer exclusively owns the listening socket.
- Accepted sockets are transferred to ClientHandler.
- TCPServer must not close accepted sockets directly.
- The active descriptor registry is non-owning.

### 4.4 ClientHandler

**Header:** `include/client_handler.hpp`
**Implementation:** `src/client_handler.cpp`

**Responsibility:** Manage one connected client.

Expected interface:

```cpp
class ClientHandler {
public:
    explicit ClientHandler(UniqueFd client_fd);

    void run();

    int getClientFd() const noexcept;
};
```

**Responsibilities:**
- Own the accepted client socket.
- Receive incoming TCP data.
- Maintain an input buffer.
- Extract complete frames using Protocol.
- Validate frame sizes.
- Process complete requests.
- Send responses.
- Detect client disconnection and socket errors.
- Perform connection cleanup.

**Important rules:**
- Each handler processes one client connection.
- No partial TCP message may be passed to the request parser.
- Oversized frames must terminate the connection.
- Socket ownership remains with ClientHandler throughout its lifetime.

### 4.5 TCPClient

**Header:** `include/tcp_client.hpp`
**Implementation:** `src/tcp_client.cpp`

**Responsibility:** Provide client-side TCP communication for testing and demonstration.

Expected interface:

```cpp
class TCPClient {
public:
    TCPClient();
    ~TCPClient();

    bool connectTo(
        const std::string& address,
        std::uint16_t port
    );

    bool sendRequest(const std::string& request);

    std::optional<Protocol::Response> receiveResponse();

    void disconnect();

    bool isConnected() const;
};
```

**Responsibilities:**
- Create and own the outgoing socket.
- Establish a TCP connection.
- Send complete protocol messages.
- Handle partial socket writes.
- Receive and buffer TCP data.
- Extract complete response frames.
- Parse responses using Protocol.
- Close the connection safely.

### 4.6 UserManager

**Header:** `include/user_manager.hpp`
**Implementation:** `src/user_manager.cpp`

**Responsibility:** Manage user authentication and online status.

Expected interface:

```cpp
class UserManager {
public:
    bool authenticate(
        const std::string& username,
        const std::string& password
    );

    bool setOnline(
        const std::string& username,
        int client_fd
    );

    void setOffline(const std::string& username);

    bool isOnline(const std::string& username) const;

    std::vector<std::string> getOnlineUsers() const;
};
```

**Responsibilities:**
- Store demonstration credentials in memory.
- Authenticate users.
- Track online users.
- Associate authenticated users with client connections.
- Prevent conflicting online login states.

**Security rules:**
- Never log plaintext passwords.
- Credentials are demonstration-only.
- Shared user data must be protected by a mutex.

### 4.7 SessionManager

**Header:** `include/session_manager.hpp`
**Implementation:** `src/session_manager.cpp`

**Responsibility:** Manage logical two-party communication sessions.

Expected interface:

```cpp
class SessionManager {
public:
    std::optional<std::string> createSession(
        const std::string& initiator,
        const std::string& target
    );

    bool acceptSession(
        const std::string& session_id,
        const std::string& username
    );

    bool rejectSession(
        const std::string& session_id,
        const std::string& username
    );

    bool endSession(
        const std::string& session_id,
        const std::string& username
    );

    std::vector<Session> getUserSessions(
        const std::string& username
    ) const;

    void cleanupUserSessions(
        const std::string& username
    );
};
```

**Responsibilities:**
- Create sessions between two users.
- Track session participants.
- Manage session state transitions.
- Authorize participant actions.
- Retrieve sessions associated with a user.
- Clean up sessions when a user disconnects.

**Session states:**

```text
PENDING
ACTIVE
REJECTED
ENDED
```

**Allowed transitions:**

```text
PENDING -> ACTIVE
PENDING -> REJECTED
ACTIVE  -> ENDED
PENDING -> ENDED
```

**Rules:**
- Only the target participant can accept or reject a pending session.
- Either participant can end an active session.
- A session contains exactly two participants.
- Group sessions are outside the project scope.
- Manager operations must be thread-safe.

*Note: The Session structure and exact method signatures must be finalized before implementation.*

### 4.8 TerminalUI

**Header:** `include/terminal.hpp`

**Responsibility:** Provide readable and thread-safe terminal output.

Expected interface:

```cpp
namespace TerminalUI {

void print_success(std::string_view message);
void print_error(std::string_view message);
void print_warning(std::string_view message);
void print_system(std::string_view message);
void print_session(std::string_view message);

}
```

**Rules:**
- All logging operations must use the shared console mutex.
- ANSI colors must be reset after formatting.
- Passwords must never appear in log messages.
- Terminal output must not interfere with socket protocol data.

## 5. Component Interaction

### 5.1 Server Startup

```text
main()
  |
  v
TCPServer::start()
  |
  v
Create listening socket
  |
  v
bind()
  |
  v
listen()
  |
  v
Accept loop
```

### 5.2 Client Connection

```text
TCPServer
  |
  | accept()
  v
Accepted socket
  |
  | move ownership
  v
ClientHandler
  |
  v
Worker thread
  |
  v
recv()
  |
  v
Protocol::extractFrames()
  |
  v
Protocol::parseRequest()
  |
  v
Request processing
  |
  v
Protocol::formatOK() / formatError()
  |
  v
send()
```

### 5.3 Graceful Shutdown

```text
TCPServer::stop()
  |
  v
Stop accepting new clients
  |
  v
Stop accept loop
  |
  v
Shutdown active client sockets
  |
  v
Join worker threads
  |
  v
Release listening socket
  |
  v
Server stopped
```

## 6. Concurrency and Synchronization

1. TCPServer controls the accept loop and worker thread collection.
2. Each worker handles one client independently.
3. UserManager protects shared user data using a mutex.
4. SessionManager protects shared session data using a mutex.
5. TerminalUI protects console output using a shared mutex.
6. Manager mutexes must not be held during blocking socket I/O.
7. Shutdown must coordinate all worker threads before destroying shared resources.

## 7. Implementation Constraints

- Language standard: C++17.
- Operating system: Linux.
- Networking API: POSIX TCP sockets.
- Concurrency model: Thread-per-client.
- Maximum concurrent clients: 20.
- Maximum message payload: 1024 bytes, excluding LF.
- Idle timeout: 45 seconds.
- Heartbeat: PING/PONG.
- Credentials: In-memory demonstration data.
- No database or persistent user storage.
- No group sessions.
- No media streaming.
- No production-grade telecom protocol implementation.

## 8. Implementation Order

The implementation should follow component dependencies:

| Order | Component | Dependency |
|---|---|---|
| 1 | UniqueFd | POSIX file descriptors |
| 2 | Protocol | Standard C++ |
| 3 | TerminalUI | Standard C++ |
| 4 | TCPServer | UniqueFd |
| 5 | ClientHandler | UniqueFd, Protocol |
| 6 | TCPClient | UniqueFd, Protocol |
| 7 | UserManager | Standard C++ |
| 8 | SessionManager | UserManager concepts |
| 9 | Integration | All components |

## 9. Contract Change Policy

- Public interfaces should remain stable once implementation begins.
- Interface changes must be reflected in this document.
- Ownership changes must be reviewed carefully.
- New features must not violate the SRS scope.
- Unit tests should be updated whenever observable behavior changes.

---

**End of Document**
