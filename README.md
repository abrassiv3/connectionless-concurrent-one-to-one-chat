# Project: Connectionless UDP Request-Response System
This project implements a lightweight, connectionless (UDP) server-client architecture in C. Originally designed as a TCP-based system, it has been refactored to use SOCK_DGRAM to reduce overhead and simplify communication for small, fast data exchanges.

# Architecture Overview
The system uses a Single-Socket Passive Open model on the server. Unlike TCP, which requires a dedicated socket per client, this UDP implementation uses one main "mailbox" to receive and respond to all incoming datagrams.

Protocol: UDP (SOCK_DGRAM)
Port: 8080 (Default)
Mode: Request-Response

# Getting Started (2-Device Setup)
To test the system across two different machines on the same network, follow these steps:
1. Identify the Server IP: On the machine running the Server, find its private IP address:
a. Linux/Mac: ip a or ifconfig
b. Windows: ipconfig
Look for the IPv4 address (e.g., 192.168.1.15).
2. Compilation: Compile both components using make all

3. Execution: Start the server first, then point the client at the server's IP:
Server terminal
./server_app

Client terminal
./client_app 192.168.1.15

Current Status & Important Notes
Refactoring from TCP to UDP
Removed: listen(), accept(), and the secondary client_sock.
Replaced: send()/recv() replaced with sendto()/recvfrom() to handle individual packet "return addresses."
Signals: SIGPIPE is still ignored as a best practice, though less critical in UDP.