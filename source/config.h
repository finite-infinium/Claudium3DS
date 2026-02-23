#pragma once
#ifndef CONFIG_H
#define CONFIG_H

/* SD card paths */
#define CONFIG_DIR      "sdmc:/3ds/Claudium3DS/"
#define CONFIG_PATH     "sdmc:/3ds/Claudium3DS/config.ini"

/* Default values */
#define DEFAULT_MODEL       "claude-sonnet-4-20250514"
#define DEFAULT_MAX_TOKENS  1024
#define DEFAULT_SYSTEM_PROMPT "You are a helpful AI assistant running on a Nintendo 3DS. Keep responses concise."

/* Key buffer sizes */
#define API_KEY_LEN     256
#define MODEL_LEN       64
#define SYSTEM_PROMPT_LEN 512
#define SESSION_TOKEN_LEN 512

typedef enum {
    THEME_DARK  = 0,
    THEME_LIGHT = 1
} AppTheme;

typedef struct {
    char     api_key[API_KEY_LEN];
    char     model[MODEL_LEN];
    char     system_prompt[SYSTEM_PROMPT_LEN];
    int      max_tokens;
    AppTheme theme;
    /* Claude Pro session token (optional) */
    char     session_token[SESSION_TOKEN_LEN];
} AppConfig;

/* Initialise config with default values */
void config_init(AppConfig *cfg);

/* Load config from SD card; returns 0 on success */
int config_load(AppConfig *cfg);

/* Save config to SD card; returns 0 on success */
int config_save(const AppConfig *cfg);

#endif /* CONFIG_H */
