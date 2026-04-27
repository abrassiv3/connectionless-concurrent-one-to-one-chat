#include <stdio.h>
#include <string.h>
#include "server.h"

void handle_send_message(Request *req, Response *res) {
    Message new_msg;
    memset(&new_msg, 0, sizeof(Message));
    new_msg.id = get_next_id("messages.dat", sizeof(Message));
    new_msg.sender_id = req->sender_id;
    new_msg.receiver_id = req->target_id;
    strncpy(new_msg.content, req->msg_content, 255);

    if (append_record("messages.dat", &new_msg, sizeof(Message)) == 0) {
        res->status = 1;
        strcpy(res->message, "Message sent successfully.");
        
        // --- NEW LOG ---
        log_event("SUCCESS", "Message ID %d sent from User %d to User %d.", 
                  new_msg.id, new_msg.sender_id, new_msg.receiver_id);
    } else {
        res->status = 0;
        strcpy(res->message, "Failed to send message.");
        
        // --- NEW LOG ---
        log_event("ERROR", "Failed to save message from User %d to messages.dat.", req->sender_id);
    }
}

void handle_view_messages(Request *req, Response *res) {
    FILE *file = fopen("messages.dat", "rb");
    res->num_messages = 0;
    if (!file) {
        res->status = 0;
        strcpy(res->message, "No messages found.");
        
        // --- NEW LOG ---
        log_event("INFO", "View messages requested for User %d, but database is empty.", req->target_id);
        return;
    }

    Message temp_msg;
    // Find messages where the requested user is either the sender or receiver
    while (fread(&temp_msg, sizeof(Message), 1, file) && res->num_messages < MAX_RECORDS) {
        if (temp_msg.sender_id == req->target_id || temp_msg.receiver_id == req->target_id) {
            res->messages_list[res->num_messages++] = temp_msg;
        }
    }
    res->status = 1;
    strcpy(res->message, "Messages retrieved.");
    
    // --- NEW LOG ---
    log_event("SUCCESS", "Retrieved %d messages involving User %d.", res->num_messages, req->target_id);
    
    fclose(file);
}

void handle_reply_message(Request *req, Response *res) {
    FILE *file = fopen("messages.dat", "rb");
    if (!file) {
        res->status = 0;
        strcpy(res->message, "Error: No messages database found.");
        return;
    }

    // 1. Find the original message we are replying to
    Message original_msg;
    int found = 0;
    while (fread(&original_msg, sizeof(Message), 1, file)) {
        if (original_msg.id == req->reply_to_id) {
            found = 1;
            break;
        }
    }
    fclose(file);

    if (!found) {
        res->status = 0;
        strcpy(res->message, "Error: Original message ID not found.");
        log_event("WARN", "Reply failed: Parent message %d not found.", req->reply_to_id);
        return;
    }

    // 2. Create the reply message
    Message new_msg;
    memset(&new_msg, 0, sizeof(Message));
    new_msg.id = get_next_id("messages.dat", sizeof(Message));
    new_msg.sender_id = req->sender_id;
    new_msg.reply_to_id = req->reply_to_id;
    strncpy(new_msg.content, req->msg_content, 255);
    
    // The receiver of the reply is whoever sent the original message
    // (Unless they are replying to their own message!)
    new_msg.receiver_id = (original_msg.sender_id == req->sender_id) ? original_msg.receiver_id : original_msg.sender_id;

    // 3. Save it
    if (append_record("messages.dat", &new_msg, sizeof(Message)) == 0) {
        res->status = 1;
        strcpy(res->message, "Reply sent successfully.");
        log_event("SUCCESS", "Reply ID %d sent from User %d to User %d (Replying to Msg %d).", 
                  new_msg.id, new_msg.sender_id, new_msg.receiver_id, new_msg.reply_to_id);
    } else {
        res->status = 0;
        strcpy(res->message, "Failed to send reply.");
        log_event("ERROR", "Failed to save reply to messages.dat.");
    }
}

void handle_search_messages(Request *req, Response *res) {
    FILE *file = fopen("messages.dat", "rb");
    res->num_messages = 0;
    if (!file) {
        res->status = 0;
        strcpy(res->message, "No messages found.");
        return;
    }

    Message temp_msg;
    // Read through all messages
    while (fread(&temp_msg, sizeof(Message), 1, file) && res->num_messages < MAX_RECORDS) {
        // If the search keyword (req->msg_content) is found inside the message content
        if (strstr(temp_msg.content, req->msg_content) != NULL) {
            res->messages_list[res->num_messages++] = temp_msg;
        }
    }
    
    res->status = 1;
    sprintf(res->message, "Found %d matching messages.", res->num_messages);
    log_event("INFO", "Message search for '%s' yielded %d results.", req->msg_content, res->num_messages);
    
    fclose(file);
}