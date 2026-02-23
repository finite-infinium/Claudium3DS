#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

void config_init(AppConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(AppConfig));
    strncpy(cfg->model, DEFAULT_MODEL, MODEL_LEN - 1);
    strncpy(cfg->system_prompt, DEFAULT_SYSTEM_PROMPT, SYSTEM_PROMPT_LEN - 1);
    cfg->max_tokens = DEFAULT_MAX_TOKENS;
    cfg->theme      = THEME_DARK;
}

/* Ensure CONFIG_DIR exists on the SD card */
static void ensure_dir(void) {
    struct stat st;
    if (stat(CONFIG_DIR, &st) != 0) {
        mkdir(CONFIG_DIR, 0777);
    }
    /* Also ensure chats subdir */
    if (stat("sdmc:/3ds/Claudium3DS/chats", &st) != 0) {
        mkdir("sdmc:/3ds/Claudium3DS/chats", 0777);
    }
}

/* Trim trailing whitespace / newlines in place */
static void trim_right(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r' || s[len-1] == ' '))
        s[--len] = '\0';
}

int config_load(AppConfig *cfg) {
    if (!cfg) return -1;
    config_init(cfg);      /* start with defaults */

    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) return -1;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        trim_right(line);
        if (line[0] == '#' || line[0] == '[' || line[0] == '\0') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;

        if (strcmp(key, "api_key") == 0)
            strncpy(cfg->api_key, val, API_KEY_LEN - 1);
        else if (strcmp(key, "model") == 0)
            strncpy(cfg->model, val, MODEL_LEN - 1);
        else if (strcmp(key, "system_prompt") == 0)
            strncpy(cfg->system_prompt, val, SYSTEM_PROMPT_LEN - 1);
        else if (strcmp(key, "max_tokens") == 0)
            cfg->max_tokens = atoi(val);
        else if (strcmp(key, "theme") == 0)
            cfg->theme = (atoi(val) == 1) ? THEME_LIGHT : THEME_DARK;
        else if (strcmp(key, "session_token") == 0)
            strncpy(cfg->session_token, val, SESSION_TOKEN_LEN - 1);
    }
    fclose(f);
    return 0;
}

int config_save(const AppConfig *cfg) {
    if (!cfg) return -1;
    ensure_dir();

    FILE *f = fopen(CONFIG_PATH, "w");
    if (!f) return -1;

    fprintf(f, "[Claudium3DS]\n");
    fprintf(f, "api_key=%s\n",       cfg->api_key);
    fprintf(f, "model=%s\n",         cfg->model);
    fprintf(f, "system_prompt=%s\n", cfg->system_prompt);
    fprintf(f, "max_tokens=%d\n",    cfg->max_tokens);
    fprintf(f, "theme=%d\n",         (int)cfg->theme);
    fprintf(f, "session_token=%s\n", cfg->session_token);
    fclose(f);
    return 0;
}
