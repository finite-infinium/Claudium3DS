#include "chat.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void chat_init(Conversation *conv) {
    if (!conv) return;
    memset(conv, 0, sizeof(Conversation));
}

int chat_add_message(Conversation *conv, MessageRole role, const char *content) {
    if (!conv || !content) return -1;
    /* Trim if we're at capacity */
    if (conv->count >= MAX_MESSAGES) {
        chat_trim(conv);
    }
    Message *m = &conv->messages[conv->count];
    m->role = role;
    strncpy(m->content, content, MAX_MSG_LEN - 1);
    m->content[MAX_MSG_LEN - 1] = '\0';
    conv->count++;
    return 0;
}

void chat_trim(Conversation *conv) {
    if (!conv || conv->count < 2) return;
    /* Remove the first two messages (one user + one assistant pair) */
    int remove = (conv->count >= 2) ? 2 : 1;
    memmove(&conv->messages[0], &conv->messages[remove],
            sizeof(Message) * (conv->count - remove));
    conv->count -= remove;
}

void chat_clear(Conversation *conv) {
    if (!conv) return;
    conv->count = 0;
    memset(conv->messages, 0, sizeof(conv->messages));
}

int chat_save(const Conversation *conv, const char *name) {
    if (!conv || !name) return -1;
    char path[256];
    snprintf(path, sizeof(path), "%s%s.txt", CHAT_SAVE_DIR, name);

    FILE *f = fopen(path, "w");
    if (!f) return -1;

    for (int i = 0; i < conv->count; i++) {
        const Message *m = &conv->messages[i];
        fprintf(f, "[%s]\n%s\n---\n",
                chat_role_str(m->role), m->content);
    }
    fclose(f);
    return 0;
}

int chat_load(Conversation *conv, const char *name) {
    if (!conv || !name) return -1;
    char path[256];
    snprintf(path, sizeof(path), "%s%s.txt", CHAT_SAVE_DIR, name);

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    chat_clear(conv);
    char line[MAX_MSG_LEN];
    char content_buf[MAX_MSG_LEN];
    MessageRole cur_role = ROLE_USER;
    int reading_content = 0;
    content_buf[0] = '\0';

    while (fgets(line, sizeof(line), f)) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';

        if (line[0] == '[') {
            /* Role header line */
            if (strncmp(line, "[user]", 6) == 0 || strncmp(line, "[USER]", 6) == 0)
                cur_role = ROLE_USER;
            else
                cur_role = ROLE_ASSISTANT;
            content_buf[0] = '\0';
            reading_content = 1;
        } else if (strcmp(line, "---") == 0) {
            /* End of message */
            if (reading_content) {
                chat_add_message(conv, cur_role, content_buf);
                content_buf[0] = '\0';
                reading_content = 0;
            }
        } else if (reading_content) {
            /* Append content line */
            if (content_buf[0] != '\0') {
                size_t blen = strlen(content_buf);
                if (blen + 1 < MAX_MSG_LEN) {
                    content_buf[blen] = '\n';
                    content_buf[blen + 1] = '\0';
                }
            }
            strncat(content_buf, line, MAX_MSG_LEN - strlen(content_buf) - 1);
        }
    }
    fclose(f);
    return 0;
}

const char *chat_role_str(MessageRole role) {
    return (role == ROLE_USER) ? "user" : "assistant";
}
