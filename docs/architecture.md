
# CTSMS — System Architecture

**Project Name:** Concurrent TCP Session Management Server  
**Version:** 1.0  
**Language:** C++17  
**Platform:** Linux  
**Architecture Style:** Multi-threaded Client-Server Architecture  
**Document Status:** Initial Design

---

## 1. Introduction

### 1.1 Purpose

This document describes the software architecture of the Concurrent TCP Session Management Server (CTSMS).

It defines the system components, their responsibilities, communication flow, resource ownership, concurrency model, and interactions between modules.

The architecture is intended to guide the implementation and integration activities of the development team.

### 1.2 System Scope

CTSMS is an educational TCP server application designed to demonstrate fundamental networking and concurrent programming concepts in C++17.

The system allows multiple terminal clients to connect to a central server, authenticate using demonstration credentials, view online users, and manage two-party logical sessions.

The project focuses on:

- TCP socket programming on Linux.
- Concurrent client handling using POSIX-compatible threads through the C++ standard library.
- Application-level message framing over TCP.
- User authentication and online-user management.
- Logical session lifecycle management.
- Resource management using RAII.
- Thread-safe access to shared application data.

CTSMS is a learning project. It is not intended to function as a production telecommunications platform, SIP server, IMS/CSCF component, PBX, or media server.

### 1.3 Design Constraints

The system follows these constraints:

| Item | Constraint |
|---|---|
| Programming language | C++17 |
| Operating system | Linux |
| Network transport | TCP |
| Maximum concurrent clients | 20 |
| Maximum message payload | 1024 bytes, excluding LF |
| Idle timeout | 45 seconds |
| Client interface | Terminal-based application |
| Server architecture | Single server, multiple clients |
| Session type | Two-party logical session |
| Data persistence | In-memory only |
| Authentication | Demonstration username/password credentials |

---

## 2. High-Level Architecture

CTSMS follows a multi-threaded client-server architecture.

The server listens for incoming TCP connections. Each accepted client is assigned a dedicated worker thread. The worker handles that client's communication, authentication, command processing, and connection lifecycle.

Shared user and session information is managed by centralized components protected by mutexes.

### 2.1 Architecture Diagram

```text
                 +----------------------+
                 |     Terminal User    |
                 +----------+-----------+
                            |
                            v
                 +----------------------+
                 |      TCPClient       |
                 |  Connect / Send /    |
                 |  Receive / Display   |
                 +----------+-----------+
                            |
                       TCP Connection
                            |
                            v
+----------------------------------------------------------+
|                         CTSMS Server                     |
|                                                          |
|  +----------------------+                                |
|  |       TCPServer      |                                |
|  | Socket / Bind /      |                                |
|  | Listen / Accept      |                                |
|  +----------+-----------+                                |
|             |                                            |
|             v                                            |
|  +----------------------+                                |
|  |    ClientHandler     |                                |
|  | One worker per       |                                |
|  | connected client     |                                |
|  +----------+-----------+                                |
|             |                                            |
|             v                                            |
|  +----------------------+                                |
|  |    CommandParser     |                                |
|  | Parse / Validate /   |                                |
|  | Serialize Messages   |                                |
|  +----------+-----------+                                |
|             |                                            |
|       +-----+-------------------+                        |
|       |                         |                        |
|       v                         v                        |
|  +----------------+      +----------------+              |
|  |  UserManager   |      | SessionManager  |              |
|  | Authentication |      | Session         |              |
|  | Online Users   |      | Lifecycle       |              |
|  +----------------+      +----------------+              |
|                                                          |
|  +----------------------+                                |
|  |       UniqueFd       |                                |
|  | RAII Socket         |                                |
|  | Ownership           |                                |
|  +----------------------+                                |
+----------------------------------------------------------+
```

### 2.2 Component Overview

The system consists of the following major components:

| Component | Responsibility |
|---|---|
| TCPServer | Creates the listening socket and accepts client connections |
| ClientHandler | Handles communication and commands for one connected client |
| TCPClient | Provides the terminal-based client application |
| CommandParser | Parses complete protocol messages and serializes responses |
| UserManager | Handles authentication and online-user information |
| SessionManager | Manages two-party logical sessions |
| UniqueFd | Provides RAII-based file descriptor ownership |

---

## 3. Component Responsibilities

### 3.1 TCPServer

**Files:**
- `include/tcp_server.hpp`
- `src/tcp_server.cpp`

TCPServer is responsible for the server's network lifecycle.

Its responsibilities include:

- Creating an IPv4 TCP listening socket.
- Binding the socket to the configured IP address and port.
- Listening for incoming connections.
- Accepting new client connections.
- Enforcing the maximum concurrent client limit.
- Creating and managing ClientHandler worker threads.
- Tracking active client file descriptors.
- Initiating graceful server shutdown.
- Joining all worker threads before termination.

TCPServer does not process application-level commands. Command processing belongs to ClientHandler and CommandParser.

### 3.2 ClientHandler

**Files:**
- `include/client_handler.hpp`
- `src/client_handler.cpp`

Each accepted client connection is handled by one ClientHandler instance running in a worker thread.

Its responsibilities include:

- Receiving bytes from the client socket.
- Accumulating incoming bytes in a receive buffer.
- Extracting complete protocol messages terminated by LF (`\n`).
- Rejecting messages that exceed the maximum payload size.
- Parsing and dispatching valid commands.
- Managing the client's authentication state.
- Sending protocol responses.
- Handling PING/PONG heartbeat messages.
- Monitoring client activity for idle timeout.
- Cleaning up user and session state after disconnection.

ClientHandler must not hold UserManager or SessionManager mutexes while performing blocking socket operations.

### 3.3 TCPClient

**Files:**
- `include/tcp_client.hpp`
- `src/tcp_client.cpp`
- `src/client_main.cpp`

TCPClient provides a simple terminal interface for users.

Its responsibilities include:

- Connecting to the CTSMS server.
- Reading commands from standard input.
- Sending protocol messages to the server.
- Receiving server responses.
- Displaying responses to the terminal.
- Handling server disconnection.
- Closing the socket safely.

The client is intended for demonstration and testing. It does not implement a graphical user interface.

### 3.4 CommandParser

**Files:**
- `include/protocol.hpp`
- `src/protocol.cpp`

CommandParser handles application-level protocol parsing and response serialization.

Its responsibilities include:

- Parsing a complete message line.
- Extracting the command name and parameters.
- Validating the message structure.
- Creating standardized success responses.
- Creating standardized error responses.

CommandParser does not perform socket operations and does not directly access UserManager or SessionManager.

It receives a complete line from ClientHandler. TCP stream buffering and message extraction are handled by ClientHandler.

### 3.5 UserManager

**Files:**
- `include/user_manager.hpp`
- `src/user_manager.cpp`

UserManager manages demonstration user credentials and online-user information.

Its responsibilities include:

- Validating usernames and passwords.
- Authenticating users.
- Tracking authenticated online users.
- Associating authenticated users with their client connections.
- Removing users from the online list after disconnection.
- Returning the current online-user list.

All shared user data must be protected by a mutex.

The demonstration credentials are stored in memory. This implementation is for educational purposes and must not be considered production-grade credential storage.

Passwords must never be written to application logs.

### 3.6 SessionManager

**Files:**
- `include/session_manager.hpp`
- `src/session_manager.cpp`

SessionManager manages two-party logical sessions between authenticated users.

Its responsibilities include:

- Creating a session between an initiator and a target user.
- Verifying that the target user is online.
- Allowing the target user to accept a pending session.
- Allowing the target user to reject a pending session.
- Allowing either participant to end an active session.
- Listing sessions involving a specific user.
- Terminating relevant sessions when a participant disconnects.

All shared session data must be protected by a mutex.

A session is a logical relationship between two users. It does not represent an actual media connection or voice/video communication channel.

### 3.7 UniqueFd

**Files:**
- `include/unique_fd.hpp`

UniqueFd is a small RAII utility that manages a Linux file descriptor.

Its responsibilities include:

- Taking ownership of a valid file descriptor.
- Closing the owned descriptor when the object is destroyed.
- Preventing accidental copying of ownership.
- Supporting move construction and move assignment.
- Providing safe access to the underlying descriptor.

UniqueFd is used by network components to reduce resource leaks and clarify socket ownership.

---

## 4. Client Connection Lifecycle

The following sequence describes a typical client connection.

1. TCPServer creates a listening socket.
2. TCPServer binds the socket to the configured address and port.
3. TCPServer starts listening for incoming connections.
4. A client connects using TCPClient.
5. TCPServer accepts the connection.
6. TCPServer checks whether the maximum client limit has been reached.
7. TCPServer creates a ClientHandler and starts a worker thread.
8. ClientHandler receives and processes protocol messages.
9. The client authenticates using the LOGIN command.
10. The client can execute authorized commands.
11. ClientHandler detects a disconnection, timeout, or shutdown.
12. UserManager removes the disconnected user from the online list.
13. SessionManager terminates relevant pending and active sessions.
14. The worker releases its socket resources.
15. TCPServer joins the worker thread during shutdown or thread cleanup.

---

## 5. Communication Protocol

### 5.1 Transport Protocol

CTSMS uses TCP as its transport protocol.

TCP provides a reliable, ordered byte stream. However, TCP does not preserve application message boundaries.

A single `recv()` call may return:

- Part of a message.
- Exactly one complete message.
- Multiple messages in one receive operation.

Therefore, ClientHandler must implement application-level message framing.

### 5.2 Message Framing

Each protocol message ends with a Line Feed character (`\n`).

The general message format is:

```text
COMMAND|PARAMETER_1|PARAMETER_2\n
```

The maximum payload size is 1024 bytes, excluding the LF terminator.

ClientHandler must buffer incoming bytes and extract complete lines before passing them to CommandParser.

An incomplete line must not be processed as a complete command.

### 5.3 Response Format

Successful responses use the following format:

```text
OK|CODE|DATA\n
```

Error responses use the following format:

```text
ERROR|ERROR_CODE|DESCRIPTION\n
```

The exact command names, parameters, response codes, and examples are defined in `docs/protocol.md`.

---

## 6. Logical Session Lifecycle

CTSMS supports two-party logical sessions.

Each session contains:

- A unique session identifier.
- An initiator username.
- A target username.
- A session state.
- Creation metadata.

### 6.1 Session States

The system defines the following states:

| State | Description |
|---|---|
| PENDING | A session has been created and is waiting for the target user |
| ACTIVE | The target user has accepted the session |
| REJECTED | The target user has rejected the session |
| ENDED | The session has been terminated |

### 6.2 State Transitions

```text
              +-----------+
              |  PENDING  |
              +-----+-----+
                    |
          +---------+---------+
          |                   |
          v                   v
    +-----------+       +-----------+
    |  ACTIVE   |       | REJECTED  |
    +-----+-----+       +-----------+
          |
          v
    +-----------+
    |   ENDED   |
    +-----------+
```

Valid user-driven transitions:

- PENDING -> ACTIVE: The target user accepts the session.
- PENDING -> REJECTED: The target user rejects the session.
- ACTIVE -> ENDED: Either participant ends the session.

On participant disconnection, the system performs cleanup by terminating relevant pending and active sessions.

### 6.3 Session Rules

- Only authenticated users can manage sessions.
- A session must have two different participants.
- The target user must be online when a session is created.
- Only the target user can accept or reject a pending session.
- Only a participant can end an active session.
- Users can list sessions in which they participate.
- A session cannot be accepted, rejected, or ended through an invalid state transition.

---

## 7. Concurrency Model

### 7.1 Thread-per-client Model

CTSMS uses a thread-per-client model.

Each accepted client connection is assigned one worker thread.

The worker thread runs the corresponding ClientHandler and manages the connection independently.

This design is suitable for the project's limited maximum of 20 concurrent clients.

### 7.2 Shared Data Protection

UserManager and SessionManager contain data shared by multiple worker threads.

Their internal data structures must be protected by mutexes.

Examples include:

- Online user records.
- User-to-connection mappings.
- Session records.
- Session state transitions.

A component must acquire its mutex only while accessing or modifying its protected data.

Mutexes must not be held while waiting for network data or performing blocking socket operations.

### 7.3 Thread Management

- Worker threads must remain joinable.
- Detached threads must not be used.
- TCPServer is responsible for coordinating worker thread shutdown.
- All worker threads must finish before the server process terminates.
- Shared state must remain valid until all workers have completed.

---

## 8. Resource Ownership and RAII

CTSMS uses RAII to manage file descriptors and other resources.

The ownership rules are:

| Resource | Owner |
|---|---|
| Listening socket | TCPServer |
| Accepted client socket | ClientHandler through UniqueFd |
| Outgoing client socket | TCPClient through UniqueFd |
| User records | UserManager |
| Session records | SessionManager |
| Worker threads | TCPServer |

A file descriptor must have one clear owner responsible for closing it.

UserManager and SessionManager do not own or close client sockets.

TCPServer may maintain a non-owning registry of active client file descriptors for connection tracking and shutdown coordination.

The registry must not be treated as an additional owner of those descriptors.

---

## 9. Heartbeat and Idle Timeout

CTSMS uses a heartbeat mechanism to detect inactive connections.

### 9.1 Heartbeat

The protocol supports:

- PING: A heartbeat request from a client.
- PONG: A heartbeat response from the server.

The exact heartbeat behavior is defined in `docs/protocol.md`.

### 9.2 Idle Timeout

A client connection is subject to a 45-second idle timeout.

ClientHandler tracks the last valid client activity using `std::chrono::steady_clock`.

When the idle timeout is exceeded, the connection is terminated and the normal disconnection cleanup procedure is executed.

The implementation must ensure that timeout monitoring does not permanently block the worker thread inside a socket receive operation.

---

## 10. Graceful Shutdown

The server must shut down in a controlled manner.

The shutdown procedure should:

1. Stop accepting new connections.
2. Exit the accept loop.
3. Stop or wake active client handlers.
4. Call `shutdown()` on active client sockets when necessary to interrupt blocking I/O.
5. Allow worker threads to perform their cleanup.
6. Join all worker threads.
7. Release the listening socket.
8. Terminate the server process.

The server must not close a client socket from multiple owners.

The ClientHandler remains responsible for releasing its accepted socket through UniqueFd.

---

## 11. Error Handling and Logging

The system must handle common network and application errors, including:

- Socket creation failure.
- Bind or listen failure.
- Connection failure.
- Client disconnection.
- Invalid protocol messages.
- Messages exceeding the maximum payload size.
- Authentication failure.
- Unauthorized commands.
- Invalid session operations.
- Idle timeout.
- Server shutdown.

Logs should provide concise information about server startup, connections, disconnections, errors, and shutdown.

Passwords and other sensitive authentication data must never be logged.

A dedicated Logger class is not required for this educational project. Standard error output or a small logging helper is sufficient.

---

## 12. Functional Requirement Mapping

The following table maps the main functional requirements to their responsible components.

| Requirement | Responsible Component |
|---|---|
| FR-001: Server startup | TCPServer |
| FR-002: Accept client connections | TCPServer |
| FR-003: Client connection | TCPClient |
| FR-004: Message framing | ClientHandler |
| FR-005: Message size validation | ClientHandler |
| FR-006: User authentication | UserManager |
| FR-007: Command authorization | ClientHandler |
| FR-008: Online user list | UserManager |
| FR-009: Create session | SessionManager |
| FR-010: Accept session | SessionManager |
| FR-011: Reject session | SessionManager |
| FR-012: End session | SessionManager |
| FR-013: List sessions | SessionManager |
| FR-014: Heartbeat | ClientHandler |
| FR-015: Idle timeout | ClientHandler |
| FR-016: Disconnection cleanup | ClientHandler, UserManager, SessionManager |
| FR-017: Graceful shutdown | TCPServer |
| FR-018: Error handling | All relevant components |

---

## 13. Project Directory Structure

```text
CTSMS/
|
|-- .vscode/
|   |-- extensions.json
|   |-- settings.json
|
|-- docs/
|   |-- architecture.md
|   |-- interface-contract.md
|   |-- protocol.md
|   |-- test-plan.md
|
|-- include/
|   |-- client_handler.hpp
|   |-- protocol.hpp
|   |-- session_manager.hpp
|   |-- tcp_client.hpp
|   |-- tcp_server.hpp
|   |-- unique_fd.hpp
|   |-- user_manager.hpp
|
|-- src/
|   |-- client_handler.cpp
|   |-- client_main.cpp
|   |-- main.cpp
|   |-- protocol.cpp
|   |-- session_manager.cpp
|   |-- tcp_client.cpp
|   |-- tcp_server.cpp
|   |-- user_manager.cpp
|
|-- tests/
|   |-- protocol_tests.cpp
|   |-- session_manager_tests.cpp
|
|-- CMakeLists.txt
|-- README.md
```

---

## 14. Future Improvements

The following features are outside the initial project scope:

- Persistent user and session storage.
- TLS encryption.
- Production-grade password hashing and credential management.
- SIP or IMS protocol integration.
- Real-time audio or video communication.
- Event-driven networking using epoll.
- A graphical user interface.
- Distributed server deployment.

These features may be considered in future versions but must not delay the implementation of the current SRS requirements.

---

## 15. Conclusion

CTSMS is designed as a small, modular, multi-threaded TCP client-server application.

The architecture separates network communication, protocol processing, user management, and session management.

RAII, clear resource ownership, mutex-protected shared state, and joinable worker threads are used to improve reliability and maintainability.

The implementation should follow this architecture while keeping the project scope aligned with the Software Requirements Specification.
