// server_main.c — Concurrent Connectionless UDP Server (fork-based)
//
// Concurrency model: process-per-request using fork().
// Each incoming UDP datagram spawns a child process to handle it.
// The parent returns to recvfrom() immediately, ready for the next request.
//
// Key additions over the iterative version:
//   - fork()   : spawns a child process per request
//   - fflush() : flushes stdout before fork so output isn't duplicated
//   - sleep(2) : simulates processing time so concurrent behaviour is visible
//   - SIGCHLD  : prevents zombie processes by reaping finished children

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include "server.h"

// ── Zombie prevention ────────────────────────────────────────────────────────
// When a child process finishes, it becomes a zombie until the parent calls
// wait(). SIGCHLD is sent to the parent when a child exits. By handling it
// with waitpid(WNOHANG) we reap children without blocking the parent.
static void sigchld_handler(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);  // disable stdout buffering

    int server_sock;
    struct sockaddr_in server_addr, client_addr;
    Request req;
    Response res;

    // ── Register SIGCHLD handler before any fork() can happen ────────────────
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    // ── Create UDP socket ─────────────────────────────────────────────────────
    printf("[PARENT] Creating UDP socket...\n");
    server_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_sock < 0) {
        perror("[PARENT] socket failed");
        return 1;
    }
    printf("[PARENT] Socket created. fd=%d\n", server_sock);

    // ── Bind to well-known address ────────────────────────────────────────────
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    printf("[PARENT] Binding to port %d on all interfaces (INADDR_ANY)...\n", PORT);
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[PARENT] bind failed");
        close(server_sock);
        return 1;
    }
    printf("[PARENT] Bind successful. Server ready.\n");
    printf("[PARENT] Awaiting requests on port %d...\n\n", PORT);

    // ── Main loop ─────────────────────────────────────────────────────────────
    while (1)
    {
        socklen_t addr_size = sizeof(client_addr);

        // ── Wait for a datagram ───────────────────────────────────────────────
        printf("[PARENT] Blocking on recvfrom()...\n");
        fflush(stdout);  // flush before fork — prevents child inheriting
                         // unflushed output and printing it twice

        ssize_t bytes = recvfrom(
            server_sock, &req, sizeof(Request), 0,
            (struct sockaddr *)&client_addr, &addr_size
        );

        printf("[PARENT] Packet received from IP: %s  port: %d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        if (bytes != (ssize_t)sizeof(Request)) {
            printf("[PARENT] Malformed request (%zd bytes, expected %zu). Discarding.\n",
                   bytes, sizeof(Request));
            continue;
        }

        printf("[PARENT] Valid request received. cmd=%d\n", req.cmd);
        printf("[PARENT] About to fork() a child process to handle this request.\n");
        fflush(stdout);  // flush again immediately before fork()

        // ── Fork a child to handle this request ──────────────────────────────
        pid_t pid = fork();

        if (pid < 0) {
            // fork() failed — handle in parent, don't crash server
            perror("[PARENT] fork() failed");
            continue;
        }

        // ════════════════════════════════════════════════════════════════════
        // CHILD PROCESS
        // ════════════════════════════════════════════════════════════════════
        if (pid == 0)
        {
            printf("[CHILD  pid=%d] Process spawned. Handling cmd=%d from %s\n",
                   getpid(), req.cmd, inet_ntoa(client_addr.sin_addr));

            // sleep(2) makes the concurrent behaviour visible:
            // while this child sleeps, the parent is already back at
            // recvfrom() and can accept another request simultaneously.
            printf("[CHILD  pid=%d] Simulating processing time (sleep 2s)...\n", getpid());
            fflush(stdout);
            sleep(2);

            // ── Process the request ──────────────────────────────────────────
            printf("[CHILD  pid=%d] Processing request now.\n", getpid());
            memset(&res, 0, sizeof(Response));

            switch (req.cmd) {
                case CMD_REGISTER_USER:
                    printf("[CHILD  pid=%d] Dispatching to handle_register()\n", getpid());
                    handle_register(&req, &res);
                    break;
                case CMD_DEREGISTER_USER:
                    printf("[CHILD  pid=%d] Dispatching to handle_deregister()\n", getpid());
                    handle_deregister(&req, &res);
                    break;
                case CMD_VIEW_USERS:
                    printf("[CHILD  pid=%d] Dispatching to handle_view_users()\n", getpid());
                    handle_view_users(&req, &res);
                    break;
                case CMD_SEND_MESSAGE:
                    printf("[CHILD  pid=%d] Dispatching to handle_send_message()\n", getpid());
                    handle_send_message(&req, &res);
                    break;
                case CMD_REPLY_MESSAGE:
                    printf("[CHILD  pid=%d] Dispatching to handle_reply_message()\n", getpid());
                    handle_reply_message(&req, &res);
                    break;
                case CMD_VIEW_MESSAGES:
                    printf("[CHILD  pid=%d] Dispatching to handle_view_messages()\n", getpid());
                    handle_view_messages(&req, &res);
                    break;
                case CMD_SEARCH_USERS:
                    printf("[CHILD  pid=%d] Dispatching to handle_search_users()\n", getpid());
                    handle_search_users(&req, &res);
                    break;
                case CMD_SEARCH_MESSAGES:
                    printf("[CHILD  pid=%d] Dispatching to handle_search_messages()\n", getpid());
                    handle_search_messages(&req, &res);
                    break;
                default:
                    printf("[CHILD  pid=%d] Unknown command %d\n", getpid(), req.cmd);
                    strcpy(res.message, "Unknown command.");
                    res.status = 0;
                    break;
            }

            // ── Send response back to client ─────────────────────────────────
            printf("[CHILD  pid=%d] Request handled. Sending response to %s...\n",
                   getpid(), inet_ntoa(client_addr.sin_addr));
            fflush(stdout);

            sendto(server_sock, &res, sizeof(Response), 0,
                   (struct sockaddr *)&client_addr, addr_size);

            printf("[CHILD  pid=%d] Response sent. Exiting child process.\n", getpid());
            fflush(stdout);

            close(server_sock);
            exit(0);   // child exits — parent's SIGCHLD handler reaps it
        }

        // ════════════════════════════════════════════════════════════════════
        // PARENT PROCESS — resumes here immediately after fork()
        // ════════════════════════════════════════════════════════════════════
        printf("[PARENT] Child pid=%d spawned. Parent returning to recvfrom().\n\n", pid);
        // Parent does NOT call wait() here — that would block it.
        // SIGCHLD handler reaps children asynchronously.
    }

    close(server_sock);
    return 0;
}
