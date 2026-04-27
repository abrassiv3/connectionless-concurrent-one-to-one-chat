#ifndef USER_H
#define USER_H

typedef struct {
    int id;
    int is_active;
    char username[50];
} User;

typedef struct {
    int id;
    int sender_id;
    int receiver_id;
    int reply_to_id;
    char content[256];
} Message;

#endif