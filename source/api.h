#pragma once
#ifndef API_H
#define API_H

#include "chat.h"
#include "config.h"

/* Anthropic API endpoint */
#define ANTHROPIC_API_URL   "https://api.anthropic.com/v1/messages"
#define ANTHROPIC_VERSION   "2023-06-01"

/* Response buffer size (bytes) — generous for Old 3DS RAM constraints */
#define API_RESPONSE_BUF    65536   /* 64 KB */

/* Timeout in seconds — generous for slow 3DS network */
#define API_TIMEOUT_SECS    120

typedef enum {
    API_OK             = 0,
    API_ERR_NO_KEY     = -1,   /* API key not configured */
    API_ERR_NETWORK    = -2,   /* curl / socket error */
    API_ERR_HTTP       = -3,   /* non-200 HTTP status */
    API_ERR_PARSE      = -4,   /* JSON parse failure */
    API_ERR_RATE_LIMIT = -5,   /* HTTP 429 */
    API_ERR_AUTH       = -6,   /* HTTP 401 */
    API_ERR_MEMORY     = -7,   /* malloc failure */
} ApiResult;

/* Initialise networking (socInit + curl_global_init) — call once at startup */
void api_init(void);

/* Clean up networking — call at shutdown */
void api_cleanup(void);

/*
 * Send the full conversation to the Anthropic API and append the
 * assistant reply to `conv`.
 *
 * out_error_msg: optional buffer (length err_len) filled with a
 *                human-readable error description on failure.
 *
 * Returns API_OK on success.
 */
ApiResult api_send(const AppConfig *cfg, Conversation *conv,
                   char *out_error_msg, int err_len);

/* Human-readable description of an ApiResult */
const char *api_result_str(ApiResult r);

#endif /* API_H */
