# QR Code Decoder over TCP

A simple C socket-based application that lets a client send QR code image files to a server, which decodes them using the ZXing Java library and returns the result.

## Overview

This project demonstrates network programming in C with a server-client model. The server listens for incoming TCP connections, accepts QR code image files from connected clients, decodes them, and sends back the extracted data or an error message. It also includes basic protections such as rate limiting, timeout handling, and event logging.

## Features

- TCP server and client communication in C
- QR image decoding through ZXing Java command-line tooling
- Concurrency support for multiple client connections
- Per-client rate limiting
- Idle connection timeout handling
- Server-side logging for connections, errors, and violations
- Simple command-line configuration for port, timeout, and limits

## Requirements

Before running the project, make sure you have:

- Linux-based environment
- GCC and Make
- Java Runtime Environment (JRE)
- ZXing Java libraries: `core.jar` and `javase.jar`

Install Java on Ubuntu with:

```bash
sudo apt update
sudo apt install default-jre
```

## Project Structure

- `QRServer.c` — server source code
- `QRClient.c` — client source code
- `Makefile` — build script
- `core.jar` — ZXing core library
- `javase.jar` — ZXing Java SE library
- `admin_log.txt` — generated server log file
- `README.md` — project documentation

## Build

From the project root, compile the server and client:

```bash
make all
```

To remove the compiled binaries:

```bash
make clean
```

## Run the Server

```bash
./QRServer -PORT <port-number> -MAX_USERS <max-clients> -RATE_MSGS <limit> -RATE_TIME <timeframe> -TIME_OUT <timeout>
```

Example:

```bash
./QRServer -PORT 2001 -MAX_USERS 3 -RATE_MSGS 5 -RATE_TIME 20 -TIME_OUT 30
```

### Supported Options

| Option | Description |
| --- | --- |
| `-PORT` | Server port to listen on |
| `-MAX_USERS` | Maximum number of simultaneous clients |
| `-RATE_MSGS` | Number of requests allowed before rate limiting |
| `-RATE_TIME` | Rate-limit time window in seconds |
| `-TIME_OUT` | Client idle timeout in seconds |

## Run the Client

```bash
./QRClient <server_ip> <server_port>
```

Example:

```bash
./QRClient 127.0.0.1 2001
```

The client will prompt for input. You can send:

- a QR image filename such as `test1.png`
- `close` to disconnect
- `shutdown` to stop the server

## Example Workflow

1. Start the server:

```bash
./QRServer -PORT 3600 -MAX_USERS 3 -RATE_MSGS 2 -RATE_TIME 60 -TIME_OUT 90
```

2. Start the client:

```bash
./QRClient 127.0.0.1 3600
```

3. Enter a QR image file path, for example:

```bash
test1.png
```

4. The client receives the server response, which may be one of the following:

- `SUCCESS (0): <decoded content>`
- `FAILURE (1): Failed to decode QR Code or file error.`
- `RATE_LIMIT (3): Rate limit exceeded.`
- `TIMEOUT (2): Server timed out your connection.`

## Logging

The server writes timestamps and events to `admin_log.txt`. Logs include:

- client connection and disconnection entries
- rate-limit violations
- decode and file errors
- timeout events

To view the log file:

```bash
cat admin_log.txt
```

## Notes

- Ensure the ZXing JAR files are present in the same directory as the compiled executables.
- The server expects valid QR image files and may reject oversized or invalid inputs.
- This project is intended primarily as a learning example for socket programming and image decoding.

## License

This project is provided for educational use. Please check with the originating author or institution before using it in other contexts.


