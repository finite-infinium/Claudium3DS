#include "input.h"

#include <string.h>
#include <stdio.h>

#ifdef __3DS__
#include <3ds.h>
#endif

void input_init(InputState *inp) {
    if (!inp) return;
    memset(inp, 0, sizeof(InputState));
}

void input_clear(InputState *inp) {
    if (!inp) return;
    inp->buf[0] = '\0';
    inp->len    = 0;
}

bool input_open_keyboard(InputState *inp, const char *hint) {
    if (!inp) return false;

#ifdef __3DS__
    SwkbdState kbd;
    swkbdInit(&kbd, SWKBD_TYPE_NORMAL, 2, INPUT_BUF_LEN - 1);
    swkbdSetHintText(&kbd, hint ? hint : "Type your message...");
    swkbdSetInitialText(&kbd, inp->buf);
    swkbdSetButton(&kbd, SWKBD_BUTTON_LEFT,   "Cancel", false);
    swkbdSetButton(&kbd, SWKBD_BUTTON_RIGHT,  "Send",   true);
    swkbdSetFeatures(&kbd, SWKBD_MULTILINE);

    char tmp[INPUT_BUF_LEN];
    SwkbdButton btn = swkbdInputText(&kbd, tmp, sizeof(tmp));

    if (btn == SWKBD_BUTTON_NONE) {
        /* User cancelled */
        return false;
    }

    strncpy(inp->buf, tmp, INPUT_BUF_LEN - 1);
    inp->buf[INPUT_BUF_LEN - 1] = '\0';
    inp->len = (int)strlen(inp->buf);
    return true;
#else
    /* Stub for host-side compilation */
    (void)hint;
    strncpy(inp->buf, "Hello from stub keyboard", INPUT_BUF_LEN - 1);
    inp->len = (int)strlen(inp->buf);
    return true;
#endif
}

int input_settings_nav(int current_field, int num_fields, u32 keys_down) {
#ifdef __3DS__
    if (keys_down & KEY_DOWN) current_field++;
    if (keys_down & KEY_UP)   current_field--;
    if (current_field < 0)          current_field = num_fields - 1;
    if (current_field >= num_fields) current_field = 0;
#else
    (void)keys_down;
    (void)num_fields;
#endif
    return current_field;
}

int input_scroll(int current_offset, u32 keys_down, int step) {
#ifdef __3DS__
    if ((keys_down & KEY_UP) || (keys_down & KEY_CPAD_UP))
        current_offset += step;
    if ((keys_down & KEY_DOWN) || (keys_down & KEY_CPAD_DOWN))
        current_offset -= step;
    if (current_offset < 0) current_offset = 0;
#else
    (void)keys_down;
    (void)step;
#endif
    return current_offset;
}
