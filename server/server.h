#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>
#include "../shared/protocol.h"

// --- Utils ---
int get_next_id(const char *filename, size_t record_size);
int append_record(const char *filename, void *record, size_t record_size);

// NEW: The logging function declaration
void log_event(const char *level, const char *format, ...);

// --- Handlers ---
void handle_register(Request *req, Response *res);
void handle_deregister(Request *req, Response *res);
void handle_view_users(Request *req, Response *res);
void handle_send_message(Request *req, Response *res);
void handle_view_messages(Request *req, Response *res);
void handle_reply_message(Request *req, Response *res);
void handle_search_users(Request *req, Response *res);
void handle_search_messages(Request *req, Response *res);

#endif