#pragma once
#ifndef CHAT_H
#define CHAT_H

#include <stddef.h>

/* Maximum number of messages in conversation history */
#define MAX_MESSAGES       64
/* Maximum bytes per message content */
#define MAX_MSG_LEN        4096
/* Directory for saved chats on SD card */
#define CHAT_SAVE_DIR      "sdmc:/3ds/Claudium3DS/chats/"

typedef enum {
    ROLE_USER      = 0,
    ROLE_ASSISTANT = 1
} MessageRole;

typedef struct {
    MessageRole role;
    char        content[MAX_MSG_LEN];
} Message;

typedef struct {
    Message messages[MAX_MESSAGES];
    int     count;
} Conversation;

/* Initialise an empty conversation */
void chat_init(Conversation *conv);

/* Append a message; returns 0 on success, -1 if buffer is full */
int chat_add_message(Conversation *conv, MessageRole role, const char *content);

/* Remove the oldest message pair to free space (sliding window) */
void chat_trim(Conversation *conv);

/* Clear all messages */
void chat_clear(Conversation *conv);

/* Save conversation to SD card (CHAT_SAVE_DIR/<name>.txt) */
int chat_save(const Conversation *conv, const char *name);

/* Load conversation from SD card */
int chat_load(Conversation *conv, const char *name);

/* Return human-readable role label */
const char *chat_role_str(MessageRole role);

#endif /* CHAT_H */
