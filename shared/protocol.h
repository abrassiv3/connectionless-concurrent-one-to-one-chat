#ifndef PROTOCOL_H
#define PROTOCOL_H
#include "user.h"

#define PORT 8080
#define MAX_RECORDS 50 // Limit for sending arrays over the network

typedef enum {
    CMD_REGISTER_USER,
    CMD_DEREGISTER_USER,
    CMD_VIEW_USERS,
    CMD_SEND_MESSAGE,
    CMD_VIEW_MESSAGES,
    CMD_REPLY_MESSAGE,
    CMD_SEARCH_USERS,      
    CMD_SEARCH_MESSAGES
} CommandType;

typedef struct {
    CommandType cmd;
    char username[50];
    int target_id;
    int sender_id;
    int reply_to_id;
    char msg_content[256];
} Request;

typedef struct {
    int status;
    char message[256];
    
    // Payloads for the "View" commands
    User users_list[MAX_RECORDS];
    int num_users;
    
    Message messages_list[MAX_RECORDS];
    int num_messages;
} Response;

#endif