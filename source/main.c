/*
 * Claudium3DS — Claude AI chat client for Nintendo 3DS
 * Entry point & main loop
 *
 * Architecture:
 *   Top screen    — conversation history (console-based, scrollable)
 *   Bottom screen — input area + touch buttons (console-based)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __3DS__
#include <3ds.h>
#endif

#include "ui.h"
#include "api.h"
#include "config.h"
#include "chat.h"
#include "input.h"

/* ------------------------------------------------------------------ */
/* Globals                                                             */
/* ------------------------------------------------------------------ */

static AppConfig    g_config;
static Conversation g_conv;
static InputState   g_input;
static AppScreen    g_screen       = SCREEN_CHAT;
static int          g_scroll       = 0;   /* 0 = bottom, positive = scroll up */
static bool         g_thinking     = false;
static int          g_settings_sel = 0;   /* selected settings field */
static char         g_status_msg[128];    /* last error / status shown */

#define SETTINGS_FIELDS 7  /* must match ui.c */

/* ------------------------------------------------------------------ */
/* Forward declarations                                                */
/* ------------------------------------------------------------------ */

static void handle_send(void);
static void handle_settings_select(void);
static void settings_edit_field(int field);

/* ------------------------------------------------------------------ */
/* Settings field editor                                               */
/* ------------------------------------------------------------------ */

static void settings_edit_field(int field) {
    InputState tmp;
    input_init(&tmp);

    switch (field) {
        case 0: /* API Key */
            strncpy(tmp.buf, g_config.api_key, INPUT_BUF_LEN - 1);
            if (input_open_keyboard(&tmp, "Enter Anthropic API Key")) {
                strncpy(g_config.api_key, tmp.buf, API_KEY_LEN - 1);
                config_save(&g_config);
                snprintf(g_status_msg, sizeof(g_status_msg), "API key saved.");
            }
            break;
        case 1: /* Model */
            strncpy(tmp.buf, g_config.model, INPUT_BUF_LEN - 1);
            if (input_open_keyboard(&tmp, "Enter model name")) {
                strncpy(g_config.model, tmp.buf, MODEL_LEN - 1);
                config_save(&g_config);
                snprintf(g_status_msg, sizeof(g_status_msg), "Model saved.");
            }
            break;
        case 2: /* System prompt */
            strncpy(tmp.buf, g_config.system_prompt, INPUT_BUF_LEN - 1);
            if (input_open_keyboard(&tmp, "Enter system prompt")) {
                strncpy(g_config.system_prompt, tmp.buf, SYSTEM_PROMPT_LEN - 1);
                config_save(&g_config);
                snprintf(g_status_msg, sizeof(g_status_msg), "System prompt saved.");
            }
            break;
        case 3: /* Max tokens */
            snprintf(tmp.buf, INPUT_BUF_LEN, "%d", g_config.max_tokens);
            if (input_open_keyboard(&tmp, "Max tokens (e.g. 1024)")) {
                int val = atoi(tmp.buf);
                if (val > 0 && val <= 8192) {
                    g_config.max_tokens = val;
                    config_save(&g_config);
                    snprintf(g_status_msg, sizeof(g_status_msg), "Max tokens saved.");
                } else {
                    snprintf(g_status_msg, sizeof(g_status_msg), "Invalid value (1-8192).");
                }
            }
            break;
        case 4: /* Theme */
            g_config.theme = (g_config.theme == THEME_DARK) ? THEME_LIGHT : THEME_DARK;
            config_save(&g_config);
            snprintf(g_status_msg, sizeof(g_status_msg), "Theme toggled.");
            break;
        case 5: /* Claude Pro token */
            strncpy(tmp.buf, g_config.session_token, INPUT_BUF_LEN - 1);
            if (input_open_keyboard(&tmp, "Paste Claude Pro session token")) {
                strncpy(g_config.session_token, tmp.buf, SESSION_TOKEN_LEN - 1);
                config_save(&g_config);
                snprintf(g_status_msg, sizeof(g_status_msg), "Session token saved.");
            }
            break;
        case 6: /* Back */
            g_screen = SCREEN_CHAT;
            break;
        default:
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Send a message to the API                                           */
/* ------------------------------------------------------------------ */

static void handle_send(void) {
    if (g_input.len == 0) return;

    /* Add user message to conversation */
    chat_add_message(&g_conv, ROLE_USER, g_input.buf);
    input_clear(&g_input);
    g_scroll   = 0;  /* auto-scroll to bottom */
    g_thinking = true;

    /* Refresh display so user sees "thinking..." */
    ui_draw_chat(&g_conv, g_scroll, g_thinking);
    ui_draw_bottom(g_input.buf, g_thinking);

#ifdef __3DS__
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
#endif

    /* Call the API */
    char err[128];
    ApiResult res = api_send(&g_config, &g_conv, err, sizeof(err));
    g_thinking = false;
    g_scroll   = 0;

    if (res != API_OK) {
        snprintf(g_status_msg, sizeof(g_status_msg), "%s", err);
    } else {
        g_status_msg[0] = '\0';
    }
}

/* ------------------------------------------------------------------ */
/* Settings screen button handler                                      */
/* ------------------------------------------------------------------ */

static void handle_settings_select(void) {
    settings_edit_field(g_settings_sel);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

#ifdef __3DS__
    /* Initialise 3DS services */
    gfxInitDefault();
    gfxSet3D(false);
    romfsInit();
    /* Ensure SD card directories exist — config_save will create them */
#endif

    /* Initialise subsystems */
    ui_init();
    api_init();

    config_init(&g_config);
    config_load(&g_config);   /* load from SD; ignore failure (use defaults) */

    chat_init(&g_conv);
    input_init(&g_input);

    g_status_msg[0] = '\0';

    /* Welcome message */
    chat_add_message(&g_conv, ROLE_ASSISTANT,
        "Welcome to Claudium3DS!\n"
        "Tap [Keyboard] or press A to start typing.\n"
        "Go to [Settings] to enter your Anthropic API key.");

    /* ---- Main loop ---- */
#ifdef __3DS__
    while (aptMainLoop())
#else
    for (int _iter = 0; _iter < 1; _iter++)  /* single iteration on host */
#endif
    {
#ifdef __3DS__
        hidScanInput();
        u32 kDown  = hidKeysDown();
        u32 kHeld  = hidKeysHeld();
        (void)kHeld;

        /* Exit on Start */
        if (kDown & KEY_START) break;

        touchPosition touch;
        hidTouchRead(&touch);
        bool touched = (kDown & KEY_TOUCH) != 0;
#else
        u32 kDown  = 0;
        (void)kDown;
        bool touched = false;
        (void)touched;
#endif

        /* ---- CHAT screen ---- */
        if (g_screen == SCREEN_CHAT) {
#ifdef __3DS__
            /* Scroll via D-pad */
            g_scroll = input_scroll(g_scroll, kDown, 1);

            /* Button A / touch to open keyboard */
            if (kDown & KEY_A) {
                input_open_keyboard(&g_input, "Type your message...");
            }

            /* Button B = send */
            if ((kDown & KEY_B) && g_input.len > 0) {
                handle_send();
            }

            /* Touch buttons */
            if (touched) {
                UiButton btn = ui_check_touch(touch.px, touch.py);
                switch (btn) {
                    case UI_BTN_KEYBOARD:
                        input_open_keyboard(&g_input, "Type your message...");
                        break;
                    case UI_BTN_SEND:
                        handle_send();
                        break;
                    case UI_BTN_SETTINGS:
                        g_screen = SCREEN_SETTINGS;
                        g_settings_sel = 0;
                        break;
                    case UI_BTN_NEW_CHAT:
                        chat_clear(&g_conv);
                        input_clear(&g_input);
                        g_scroll = 0;
                        g_status_msg[0] = '\0';
                        chat_add_message(&g_conv, ROLE_ASSISTANT,
                            "New chat started. Tap [Keyboard] to type.");
                        break;
                    case UI_BTN_SCROLL_UP:
                        g_scroll++;
                        break;
                    case UI_BTN_SCROLL_DN:
                        if (g_scroll > 0) g_scroll--;
                        break;
                    default:
                        break;
                }
            }
#endif

            ui_draw_chat(&g_conv, g_scroll, g_thinking);
            ui_draw_bottom(g_input.buf, g_thinking);
            if (g_status_msg[0])
                ui_show_status(g_status_msg);

        /* ---- SETTINGS screen ---- */
        } else if (g_screen == SCREEN_SETTINGS) {
#ifdef __3DS__
            g_settings_sel = input_settings_nav(g_settings_sel, SETTINGS_FIELDS, kDown);

            if (kDown & KEY_A) {
                handle_settings_select();
            }
            if (kDown & KEY_B) {
                g_screen = SCREEN_CHAT;
            }
#endif
            ui_draw_settings(&g_config, g_settings_sel);
            if (g_status_msg[0])
                ui_show_status(g_status_msg);
        }

#ifdef __3DS__
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
#endif
    }

    /* ---- Cleanup ---- */
    api_cleanup();

#ifdef __3DS__
    romfsExit();
    gfxExit();
#endif

    return 0;
}
