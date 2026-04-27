#include <stdio.h>
#include <string.h>
#include "server.h"

void handle_register(Request *req, Response *res) {
    User new_user;
    memset(&new_user, 0, sizeof(User));
    new_user.is_active = 1;
    strncpy(new_user.username, req->username, 49);
    new_user.id = get_next_id("users.dat", sizeof(User));

    if (append_record("users.dat", &new_user, sizeof(User)) == 0) {
        res->status = 1;
        sprintf(res->message, "Success: User '%s' registered with ID %d.", new_user.username, new_user.id);
        
        // --- NEW LOG ---
        log_event("SUCCESS", "Registered new user '%s' with ID %d", new_user.username, new_user.id);
    } else {
        res->status = 0;
        strcpy(res->message, "Error: Could not save user.");
        
        // --- NEW LOG ---
        log_event("ERROR", "Failed to write user '%s' to users.dat", new_user.username);
    }
}

void handle_deregister(Request *req, Response *res) {
    FILE *file = fopen("users.dat", "rb+");
    if (!file) {
        res->status = 0;
        strcpy(res->message, "Error: Database empty.");
        
        // --- NEW LOG ---
        log_event("WARN", "Deregister failed: users.dat is empty or missing.");
        return;
    }

    User temp_user;
    long offset = (req->target_id - 1) * sizeof(User);
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);

    if (offset >= 0 && offset < file_size) {
        fseek(file, offset, SEEK_SET);
        fread(&temp_user, sizeof(User), 1, file);

        if (temp_user.is_active == 1) {
            temp_user.is_active = 0;
            fseek(file, offset, SEEK_SET);
            fwrite(&temp_user, sizeof(User), 1, file);
            res->status = 1;
            sprintf(res->message, "Success: User %d deregistered.", temp_user.id);
            
            // --- NEW LOG ---
            log_event("SUCCESS", "Deregistered user '%s' (ID: %d)", temp_user.username, temp_user.id);
        } else {
            res->status = 0;
            strcpy(res->message, "User is already inactive.");
            
            // --- NEW LOG ---
            log_event("INFO", "Deregister skipped: User ID %d is already inactive.", req->target_id);
        }
    } else {
        res->status = 0;
        strcpy(res->message, "User not found.");
        
        // --- NEW LOG ---
        log_event("WARN", "Deregister failed: User ID %d not found.", req->target_id);
    }
    fclose(file);
}

void handle_view_users(Request *req, Response *res) {
    FILE *file = fopen("users.dat", "rb");
    res->num_users = 0;
    if (!file) {
        res->status = 0;
        strcpy(res->message, "No users found.");
        
        // --- NEW LOG ---
        log_event("INFO", "View users requested, but database is empty.");
        return;
    }

    User temp_user;
    while (fread(&temp_user, sizeof(User), 1, file) && res->num_users < MAX_RECORDS) {
        res->users_list[res->num_users++] = temp_user;
    }
    res->status = 1;
    strcpy(res->message, "Users retrieved successfully.");
    
    // --- NEW LOG ---
    log_event("SUCCESS", "Retrieved %d users for viewing.", res->num_users);
    fclose(file);
}

void handle_search_users(Request *req, Response *res) {
    FILE *file = fopen("users.dat", "rb");
    res->num_users = 0;
    if (!file) {
        res->status = 0;
        strcpy(res->message, "No users found.");
        return;
    }

    User temp_user;
    // Read through all users
    while (fread(&temp_user, sizeof(User), 1, file) && res->num_users < MAX_RECORDS) {
        // If the search term (req->username) is found inside the user's name
        if (strstr(temp_user.username, req->username) != NULL) {
            res->users_list[res->num_users++] = temp_user;
        }
    }
    
    res->status = 1;
    sprintf(res->message, "Found %d matching users.", res->num_users);
    log_event("INFO", "User search for '%s' yielded %d results.", req->username, res->num_users);
    
    fclose(file);
}