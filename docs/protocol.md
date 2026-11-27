# CTSMS Application Protocol

**Version:** 1.0
**Protocol:** TCP Text-Based Protocol
**Encoding:** ASCII-compatible text
**Message Delimiter:** Line Feed (`\n`)

## 1. Overview

CTSMS uses a simple line-based text protocol over TCP.

TCP provides a byte stream rather than message boundaries. Therefore, each application message must end with a newline character (`\n`).

Each request and response is transmitted as one complete line.

## 2. Message Framing

- Each message must end with `\n` (LF).
- The maximum message payload is 1024 bytes, excluding the LF delimiter.
- A message exceeding this limit is considered invalid.
- A message may arrive across multiple `recv()` calls.
- Multiple messages may arrive in a single `recv()` call.
- The receiver must buffer incoming bytes and extract complete frames before parsing them.

Example:

```text
PING\n
LOGIN|alice|password123\n
```

## 3. Request Format

General syntax:

```text
COMMAND[|PARAMETER1|PARAMETER2|...]\n
```

- Commands are written in uppercase.
- The `|` character separates parameters.
- Parameters are interpreted according to the command.
- Empty or missing parameters are subject to command-specific validation.
- The `|` character is reserved as a parameter delimiter and cannot be used inside an individual request parameter.

## 4. Supported Commands

| Command | Parameters | Description |
|---|---:|---|
| `LOGIN` | 2 | Authenticate a user |
| `LOGOUT` | 0 | Log out the current user |
| `LIST_USERS` | 0 | Retrieve online users |
| `CREATE_SESSION` | 1 | Send a session invitation |
| `ACCEPT_SESSION` | 1 | Accept a pending session |
| `REJECT_SESSION` | 1 | Reject a pending session |
| `END_SESSION` | 1 | Terminate an active session |
| `LIST_SESSIONS` | 0 | Retrieve relevant sessions |
| `PING` | 0 | Check connection health |
| `QUIT` | 0 | Request connection termination |

### Request Examples

```text
LOGIN|alice|password123
LOGOUT
LIST_USERS
CREATE_SESSION|bob
ACCEPT_SESSION|1001
REJECT_SESSION|1001
END_SESSION|1001
LIST_SESSIONS
PING
QUIT
```

## 5. Response Format

### 5.1 Success Response

General syntax:

```text
OK|CODE[|DATA...]\n
```

The data field is optional.

Examples:

```text
OK|LOGIN_SUCCESS
OK|SESSION_CREATED|1001
OK|PONG
OK|LOGOUT_SUCCESS
```

### 5.2 Error Response

General syntax:

```text
ERROR|ERROR_CODE|DESCRIPTION\n
```

Examples:

```text
ERROR|INVALID_CREDENTIALS|Login failed
ERROR|USER_OFFLINE|Target user is offline
ERROR|INVALID_COMMAND|Unsupported command
ERROR|INVALID_PARAMETERS|Incorrect parameter count
```

## 6. Heartbeat

The client sends:

```text
PING\n
```

The server responds:

```text
OK|PONG\n
```

A valid application message received from the client resets the idle timeout.

If the server does not receive a valid application message for 45 seconds, it closes the connection.

## 7. Connection Termination

The client may send:

```text
QUIT\n
```

The server terminates the connection after processing the request.

A connection may also terminate because of:
- Client disconnection.
- Network or socket errors.
- Idle timeout.
- Oversized or invalid message framing.
- Server shutdown.

## 8. Protocol Error Handling

The server must handle invalid requests without terminating the entire server process.

Examples include:
- Empty messages.
- Unsupported commands.
- Incorrect parameter counts.
- Invalid parameters.
- Messages exceeding the maximum payload size.

An oversized frame causes the affected client connection to be closed.

## 9. Design Constraints

- TCP is a byte-stream transport; message boundaries are defined by LF.
- The protocol does not include request IDs.
- Sessions are logical relationships between two users, not group sessions.
- Authentication and session state are managed by the server.
- Passwords must never be written to application logs.
