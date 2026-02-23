#pragma once
#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "chat.h"
#include "config.h"

/* Top screen dimensions */
#define TOP_WIDTH   400
#define TOP_HEIGHT  240
/* Bottom screen dimensions */
#define BOT_WIDTH   320
#define BOT_HEIGHT  240

/* Console column/row limits (approximate for 8×10 font) */
#define TOP_COLS    50
#define TOP_ROWS    24
#define BOT_COLS    40
#define BOT_ROWS    24

/* Maximum lines in the top-screen scroll buffer */
#define SCROLL_BUF_LINES  512
#define SCROLL_LINE_LEN   (TOP_COLS + 1)

/* Visible chat rows (excluding status bar) */
#define CHAT_VISIBLE_ROWS  21

/*
 * Touch button IDs — returned by ui_check_touch()
 * UI_BTN_NONE means no button was hit.
 */
typedef enum {
    UI_BTN_NONE      = 0,
    UI_BTN_SEND      = 1,
    UI_BTN_KEYBOARD  = 2,
    UI_BTN_SETTINGS  = 3,
    UI_BTN_NEW_CHAT  = 4,
    UI_BTN_SCROLL_UP = 5,
    UI_BTN_SCROLL_DN = 6,
} UiButton;

/* App-level screen states */
typedef enum {
    SCREEN_CHAT     = 0,
    SCREEN_SETTINGS = 1,
} AppScreen;

/* Thinking indicator state */
typedef struct {
    bool  active;
    int   frame;      /* animation tick */
} ThinkingState;

/* Initialise consoles for top and bottom screens */
void ui_init(void);

/* Refresh the top screen with the current conversation */
void ui_draw_chat(const Conversation *conv, int scroll_offset, bool thinking);

/* Refresh the bottom screen controls */
void ui_draw_bottom(const char *input_buf, bool thinking);

/* Refresh the settings screen on the bottom display */
void ui_draw_settings(const AppConfig *cfg, int selected_field);

/* Check if a touch event hit a button — pass hidTouchRead output */
UiButton ui_check_touch(int tx, int ty);

/* Show a short status / error message on the bottom screen */
void ui_show_status(const char *msg);

/* Clear both screens */
void ui_clear_all(void);

/* Wrap a string into multiple lines, stored in `lines`.
 * Returns the number of lines produced. */
int ui_word_wrap(const char *text, int max_cols, char lines[][SCROLL_LINE_LEN], int max_lines);

#endif /* UI_H */
