#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../shared/protocol.h"

/* ── Low-level send/receive ── */

/*
 * Sends a Request and blocks until a Response arrives.
 * Returns 1 on success, 0 on network failure.
 */
static int transact(int sock, struct sockaddr_in *server_addr,
                    Request *req, Response *res)
{
    socklen_t addr_size = sizeof(*server_addr);

    if (sendto(sock, req, sizeof(Request), 0,
               (struct sockaddr *)server_addr, addr_size) < 0) {
        perror("sendto failed");
        return 0;
    }

    if (recvfrom(sock, res, sizeof(Response), 0,
                 (struct sockaddr *)server_addr, &addr_size) < 0) {
        perror("recvfrom failed");
        return 0;
    }

    return 1;
}

/* ── Input helpers ─-*/

/* Reads a line from stdin, strips the newline, returns 0 on EOF. */
static int read_line(const char *prompt, char *buf, int size)
{
    printf("%s", prompt);
    if (!fgets(buf, size, stdin)) return 0;
    buf[strcspn(buf, "\n")] = '\0';
    return 1;
}

/* Reads an integer from stdin. Loops until valid input is given. */
static int read_int(const char *prompt)
{
    char buf[32];
    int val;
    while (1) {
        read_line(prompt, buf, sizeof(buf));
        if (sscanf(buf, "%d", &val) == 1) return val;
        printf("  Please enter a valid number.\n");
    }
}

/* ── Command handlers (client side) ───*/

static void do_register(int sock, struct sockaddr_in *addr)
{
    Request req;
    Response res;
    memset(&req, 0, sizeof(req));

    req.cmd = CMD_REGISTER_USER;
    read_line("  Username: ", req.username, sizeof(req.username));

    if (transact(sock, addr, &req, &res))
        printf("  Server: %s\n", res.message);
}

static void do_deregister(int sock, struct sockaddr_in *addr)
{
    Request req;
    Response res;
    memset(&req, 0, sizeof(req));

    req.cmd = CMD_DEREGISTER_USER;
    req.target_id = read_int("  User ID to deregister: ");

    if (transact(sock, addr, &req, &res))
        printf("  Server: %s\n", res.message);
}

static void do_view_users(int sock, struct sockaddr_in *addr)
{
    Request req;
    Response res;
    memset(&req, 0, sizeof(req));

    req.cmd = CMD_VIEW_USERS;

    if (!transact(sock, addr, &req, &res)) return;

    printf("  Server: %s\n", res.message);
    if (res.num_users == 0) {
        printf("  (no users)\n");
        return;
    }

    printf("  %-6s %-20s %s\n", "ID", "Username", "Status");
    printf("  %-6s %-20s %s\n", "------", "--------------------", "--------");
    for (int i = 0; i < res.num_users; i++) {
        User *u = &res.users_list[i];
        printf("  %-6d %-20s %s\n",
               u->id, u->username, u->is_active ? "active" : "inactive");
    }
}

static void do_search_users(int sock, struct sockaddr_in *addr)
{
    Request req;
    Response res;
    memset(&req, 0, sizeof(req));

    req.cmd = CMD_SEARCH_USERS;
    read_line("  Search term: ", req.username, sizeof(req.username));

    if (!transact(sock, addr, &req, &res)) return;

    printf("  Server: %s\n", res.message);
    for (int i = 0; i < res.num_users; i++) {
        User *u = &res.users_list[i];
        printf("  [%d] %s (%s)\n",
               u->id, u->username, u->is_active ? "active" : "inactive");
    }
}

static void do_send_message(int sock, struct sockaddr_in *addr)
{
    Request req;
    Response res;
    memset(&req, 0, sizeof(req));

    req.cmd       = CMD_SEND_MESSAGE;
    req.sender_id = read_int("  Your user ID:      ");
    req.target_id = read_int("  Recipient user ID: ");
    read_line("  Message: ", req.msg_content, sizeof(req.msg_content));

    if (transact(sock, addr, &req, &res))
        printf("  Server: %s\n", res.message);
}

static void do_reply_message(int sock, struct sockaddr_in *addr)
{
    Request req;
    Response res;
    memset(&req, 0, sizeof(req));

    req.cmd         = CMD_REPLY_MESSAGE;
    req.sender_id   = read_int("  Your user ID:          ");
    req.reply_to_id = read_int("  Message ID to reply to: ");
    read_line("  Reply text: ", req.msg_content, sizeof(req.msg_content));

    if (transact(sock, addr, &req, &res))
        printf("  Server: %s\n", res.message);
}

/* Shared display logic for message lists */
static void print_messages(Response *res)
{
    if (res->num_messages == 0) {
        printf("  (no messages)\n");
        return;
    }
    for (int i = 0; i < res->num_messages; i++) {
        Message *m = &res->messages_list[i];
        if (m->reply_to_id != 0)
            printf("  [Msg #%d] From %d → To %d  (reply to #%d)\n"
                   "    %s\n",
                   m->id, m->sender_id, m->receiver_id, m->reply_to_id,
                   m->content);
        else
            printf("  [Msg #%d] From %d → To %d\n    %s\n",
                   m->id, m->sender_id, m->receiver_id, m->content);
    }
}

static void do_view_messages(int sock, struct sockaddr_in *addr)
{
    Request req;
    Response res;
    memset(&req, 0, sizeof(req));

    req.cmd       = CMD_VIEW_MESSAGES;
    req.target_id = read_int("  User ID to view messages for: ");

    if (!transact(sock, addr, &req, &res)) return;

    printf("  Server: %s\n", res.message);
    print_messages(&res);
}

static void do_search_messages(int sock, struct sockaddr_in *addr)
{
    Request req;
    Response res;
    memset(&req, 0, sizeof(req));

    req.cmd = CMD_SEARCH_MESSAGES;
    read_line("  Keyword: ", req.msg_content, sizeof(req.msg_content));

    if (!transact(sock, addr, &req, &res)) return;

    printf("  Server: %s\n", res.message);
    print_messages(&res);
}

/* ── Menu ─-*/

static void print_menu(void)
{
    printf("\n╔══════════════════════════════╗\n");
    printf("║       Messaging Client       ║\n");
    printf("╠══════════════════════════════╣\n");
    printf("║  1. Register user            ║\n");
    printf("║  2. Deregister user          ║\n");
    printf("║  3. View all users           ║\n");
    printf("║  4. Search users             ║\n");
    printf("║  5. Send message             ║\n");
    printf("║  6. Reply to message         ║\n");
    printf("║  7. View messages (by user)  ║\n");
    printf("║  8. Search messages          ║\n");
    printf("║  0. Exit                     ║\n");
    printf("╚══════════════════════════════╝\n");
    printf("Choice: ");
}

/* ── Entry point ──────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }

    /* Set up UDP socket */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    printf("Connected to server at %s:%d\n", argv[1], PORT);

    char choice_buf[8];
    int  choice;

    while (1) {
        print_menu();
        if (!read_line("", choice_buf, sizeof(choice_buf))) break;
        if (sscanf(choice_buf, "%d", &choice) != 1) continue;

        switch (choice) {
            case 1: do_register(sock, &server_addr);       break;
            case 2: do_deregister(sock, &server_addr);     break;
            case 3: do_view_users(sock, &server_addr);     break;
            case 4: do_search_users(sock, &server_addr);   break;
            case 5: do_send_message(sock, &server_addr);   break;
            case 6: do_reply_message(sock, &server_addr);  break;
            case 7: do_view_messages(sock, &server_addr);  break;
            case 8: do_search_messages(sock, &server_addr);break;
            case 0:
                printf("Goodbye.\n");
                close(sock);
                return 0;
            default:
                printf("  Unknown option.\n");
        }
    }

    close(sock);
    return 0;
}