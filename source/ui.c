#include "ui.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __3DS__
#include <3ds.h>
#endif

/* ---- Console handles ---- */
#ifdef __3DS__
static PrintConsole top_console;
static PrintConsole bot_console;
#endif

/* ---- Touch button hit-boxes (bottom screen coordinates) ---- */
typedef struct { int x, y, w, h; UiButton id; } TouchBtn;

static const TouchBtn s_buttons[] = {
    /* x    y    w    h    id */
    {   2, 200,  74,  18,  UI_BTN_KEYBOARD  },
    {  82, 200,  74,  18,  UI_BTN_SEND      },
    { 162, 200,  74,  18,  UI_BTN_SETTINGS  },
    { 242, 200,  74,  18,  UI_BTN_NEW_CHAT  },
    {   2, 174,  74,  20,  UI_BTN_SCROLL_UP },
    {  82, 174,  74,  20,  UI_BTN_SCROLL_DN },
};
static const int s_button_count = (int)(sizeof(s_buttons) / sizeof(s_buttons[0]));

/* ---- Initialise ---- */

void ui_init(void) {
#ifdef __3DS__
    consoleInit(GFX_TOP, &top_console);
    consoleInit(GFX_BOTTOM, &bot_console);
#endif
}

/* ---- Select a console ---- */

static void select_top(void) {
#ifdef __3DS__
    consoleSelect(&top_console);
#endif
}

static void select_bot(void) {
#ifdef __3DS__
    consoleSelect(&bot_console);
#endif
}

/* ---- Word-wrap ---- */

int ui_word_wrap(const char *text, int max_cols,
                 char lines[][SCROLL_LINE_LEN], int max_lines) {
    if (!text || max_cols <= 0 || !lines || max_lines <= 0) return 0;

    int line_idx = 0;
    int col      = 0;
    int src      = 0;
    int line_start = 0;

    while (text[src] && line_idx < max_lines) {
        if (text[src] == '\n') {
            /* Copy current line and advance */
            int len = src - line_start;
            if (len > max_cols) len = max_cols;
            strncpy(lines[line_idx], text + line_start, len);
            lines[line_idx][len] = '\0';
            line_idx++;
            src++;
            line_start = src;
            col = 0;
            continue;
        }

        col++;
        if (col >= max_cols) {
            /* Find last space to break on */
            int break_pos = src;
            while (break_pos > line_start && text[break_pos] != ' ')
                break_pos--;

            if (break_pos == line_start) {
                /* No space found — hard break */
                break_pos = src;
            }

            int len = break_pos - line_start;
            if (len > max_cols) len = max_cols;
            strncpy(lines[line_idx], text + line_start, len);
            lines[line_idx][len] = '\0';
            line_idx++;

            /* Skip the space at break_pos if we broke there */
            line_start = break_pos;
            if (text[line_start] == ' ') line_start++;
            src = line_start;
            col = 0;
            continue;
        }
        src++;
    }

    /* Flush remaining text */
    if (line_start < src && line_idx < max_lines) {
        int len = src - line_start;
        if (len > max_cols) len = max_cols;
        strncpy(lines[line_idx], text + line_start, len);
        lines[line_idx][len] = '\0';
        line_idx++;
    }

    return line_idx;
}

/* ---- Top screen chat ---- */

void ui_draw_chat(const Conversation *conv, int scroll_offset, bool thinking) {
    select_top();

    /* Build a flat array of wrapped lines with role prefix */
    static char scroll_buf[SCROLL_BUF_LINES][SCROLL_LINE_LEN];
    int total_lines = 0;

    /* Status bar (row 0) */
    snprintf(scroll_buf[total_lines], SCROLL_LINE_LEN,
             "\x1b[36m Claudium3DS | %d msg(s)\x1b[0m",
             conv ? conv->count : 0);
    total_lines++;

    /* Separator */
    char sep[SCROLL_LINE_LEN];
    memset(sep, '-', TOP_COLS);
    sep[TOP_COLS] = '\0';
    strncpy(scroll_buf[total_lines++], sep, SCROLL_LINE_LEN - 1);

    if (conv) {
        static char wrap_lines[64][SCROLL_LINE_LEN];
        for (int i = 0; i < conv->count && total_lines < SCROLL_BUF_LINES - 10; i++) {
            const Message *m = &conv->messages[i];
            /* Prefix line */
            char prefix[SCROLL_LINE_LEN];
            if (m->role == ROLE_USER) {
                snprintf(prefix, SCROLL_LINE_LEN, "\x1b[33mYou:\x1b[0m");
            } else {
                snprintf(prefix, SCROLL_LINE_LEN, "\x1b[32mClaude:\x1b[0m");
            }
            strncpy(scroll_buf[total_lines], prefix, SCROLL_LINE_LEN - 1);
            total_lines++;

            /* Word-wrapped content */
            int nlines = ui_word_wrap(m->content, TOP_COLS - 2, wrap_lines, 64);
            for (int j = 0; j < nlines && total_lines < SCROLL_BUF_LINES - 2; j++) {
                snprintf(scroll_buf[total_lines], SCROLL_LINE_LEN,
                         "  %s", wrap_lines[j]);
                total_lines++;
            }
            /* Blank line between messages */
            if (total_lines < SCROLL_BUF_LINES - 1) {
                scroll_buf[total_lines][0] = '\0';
                total_lines++;
            }
        }
    }

    if (thinking && total_lines < SCROLL_BUF_LINES - 1) {
        strncpy(scroll_buf[total_lines], "\x1b[35mClaude is thinking...\x1b[0m",
                SCROLL_LINE_LEN - 1);
        total_lines++;
    }

    /* Calculate visible window */
    int max_start = total_lines - CHAT_VISIBLE_ROWS;
    if (max_start < 0) max_start = 0;
    /* scroll_offset of 0 = bottom (latest); positive = scroll up */
    int start = max_start - scroll_offset;
    if (start < 0) start = 0;

    /* Render */
#ifdef __3DS__
    consoleClear();
    for (int row = 0; row < CHAT_VISIBLE_ROWS && (start + row) < total_lines; row++) {
        printf("%s\n", scroll_buf[start + row]);
    }
#endif
    (void)scroll_buf; /* suppress unused warning on host */
    (void)start;
}

/* ---- Bottom screen controls ---- */

void ui_draw_bottom(const char *input_buf, bool thinking) {
    select_bot();
#ifdef __3DS__
    consoleClear();

    /* Input preview area */
    printf("\x1b[0;0H");
    printf("\x1b[36m--- Input ---\x1b[0m\n");

    if (thinking) {
        printf("\x1b[35mClaude is thinking...\x1b[0m\n");
    } else {
        /* Show up to 3 lines of input preview */
        static char wrap[3][SCROLL_LINE_LEN];
        int n = ui_word_wrap(input_buf ? input_buf : "", BOT_COLS - 2, wrap, 3);
        for (int i = 0; i < n; i++) printf("  %s\n", wrap[i]);
        if (n == 0) printf("  (empty)\n");
    }

    /* Scroll buttons row (row 20 in 0-indexed = y≈174px) */
    printf("\x1b[20;0H");
    printf("\x1b[42m[Scrl^]\x1b[0m ");
    printf("\x1b[42m[ScrlV]\x1b[0m ");
    printf("\n");

    /* Main buttons row (row 22 = y≈200px) */
    printf("\x1b[22;0H");
    printf("\x1b[44m[Keyboard]\x1b[0m ");
    printf("\x1b[42m[Send]\x1b[0m ");
    printf("\x1b[41m[Settings]\x1b[0m ");
    printf("\x1b[43m[New Chat]\x1b[0m\n");
#endif
    (void)input_buf;
    (void)thinking;
}

/* ---- Settings screen ---- */

void ui_draw_settings(const AppConfig *cfg, int selected_field) {
    select_bot();
#ifdef __3DS__
    consoleClear();
    printf("\x1b[0;0H");
    printf("\x1b[36m=== Settings ===\x1b[0m\n");

    const char *fields[] = {
        "API Key",
        "Model",
        "System Prompt",
        "Max Tokens",
        "Theme",
        "Claude Pro Token",
        "< Back >"
    };
    int nfields = 7;

    for (int i = 0; i < nfields; i++) {
        if (i == selected_field)
            printf("\x1b[43m > %s\x1b[0m\n", fields[i]);
        else
            printf("   %s\n", fields[i]);
    }

    printf("\n");
    if (cfg) {
        /* Show masked key */
        int klen = (int)strlen(cfg->api_key);
        if (klen > 0) {
            printf(" Key: ****%s\n",
                   klen > 4 ? cfg->api_key + klen - 4 : "****");
        } else {
            printf(" Key: (not set)\n");
        }
        printf(" Model: %s\n", cfg->model);
        printf(" Tokens: %d\n", cfg->max_tokens);
    }

    printf("\n\x1b[37mUse D-pad to navigate.\x1b[0m\n");
    printf("\x1b[37mA = select/edit, B = back\x1b[0m\n");
#endif
    (void)cfg;
    (void)selected_field;
}

/* ---- Touch detection ---- */

UiButton ui_check_touch(int tx, int ty) {
    for (int i = 0; i < s_button_count; i++) {
        const TouchBtn *b = &s_buttons[i];
        if (tx >= b->x && tx < b->x + b->w &&
            ty >= b->y && ty < b->y + b->h)
            return b->id;
    }
    return UI_BTN_NONE;
}

/* ---- Status message ---- */

void ui_show_status(const char *msg) {
    select_bot();
#ifdef __3DS__
    /* Print at bottom of screen */
    printf("\x1b[23;0H\x1b[41m%-40s\x1b[0m", msg ? msg : "");
#endif
    (void)msg;
}

/* ---- Clear all ---- */

void ui_clear_all(void) {
#ifdef __3DS__
    select_top();
    consoleClear();
    select_bot();
    consoleClear();
#endif
}
