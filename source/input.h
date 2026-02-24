#pragma once
#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#ifdef __3DS__
#include <3ds.h>
#endif
#include "ui.h"
#include "config.h"

/* Max length of the input text buffer */
#define INPUT_BUF_LEN 512

typedef struct {
    char  buf[INPUT_BUF_LEN];
    int   len;
} InputState;

/* Initialise input state */
void input_init(InputState *inp);

/* Clear the input buffer */
void input_clear(InputState *inp);

/*
 * Open the system software keyboard and fill inp->buf with the result.
 * Returns true if the user confirmed (text may be empty), false if cancelled.
 */
bool input_open_keyboard(InputState *inp, const char *hint);

/*
 * Process a d-pad / button scan result for the settings screen.
 * Returns the new selected_field value.
 */
int input_settings_nav(int current_field, int num_fields, u32 keys_down);

/*
 * Handle D-pad scroll input.
 * Returns the new scroll_offset (clamped to >= 0).
 */
int input_scroll(int current_offset, u32 keys_down, int step);

#endif /* INPUT_H */
