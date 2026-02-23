#include "api.h"
#include "cJSON.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __3DS__
#include <3ds.h>
#include <curl/curl.h>

/* libctru socket heap */
#define SOC_BUFFER_SIZE  (0x100000)  /* 1 MB */
static u32  *soc_buffer = NULL;
static bool  soc_initialised = false;
#else
/* Host-side stub for unit testing */
#include <curl/curl.h>
#endif

/* ---------- Response accumulation buffer ---------- */

typedef struct {
    char  *data;
    size_t size;
    size_t capacity;
} ResponseBuf;

static int response_buf_init(ResponseBuf *b, size_t initial_cap) {
    b->data = (char*)malloc(initial_cap);
    if (!b->data) return -1;
    b->data[0]  = '\0';
    b->size     = 0;
    b->capacity = initial_cap;
    return 0;
}

static void response_buf_free(ResponseBuf *b) {
    if (b->data) { free(b->data); b->data = NULL; }
    b->size = b->capacity = 0;
}

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t incoming = size * nmemb;
    ResponseBuf *b  = (ResponseBuf*)userdata;

    if (b->size + incoming + 1 > b->capacity) {
        /* Limit to API_RESPONSE_BUF to protect Old 3DS memory */
        if (b->capacity >= API_RESPONSE_BUF) return incoming; /* silently drop */
        size_t new_cap = b->capacity * 2;
        if (new_cap > API_RESPONSE_BUF) new_cap = API_RESPONSE_BUF;
        char *tmp = (char*)realloc(b->data, new_cap);
        if (!tmp) return 0;
        b->data     = tmp;
        b->capacity = new_cap;
    }

    memcpy(b->data + b->size, ptr, incoming);
    b->size += incoming;
    b->data[b->size] = '\0';
    return incoming;
}

/* ---------- Init / Cleanup ---------- */

void api_init(void) {
#ifdef __3DS__
    if (!soc_initialised) {
        soc_buffer = (u32*)memalign(0x1000, SOC_BUFFER_SIZE);
        if (soc_buffer) {
            Result rc = socInit(soc_buffer, SOC_BUFFER_SIZE);
            if (R_SUCCEEDED(rc)) soc_initialised = true;
        }
    }
#endif
    curl_global_init(CURL_GLOBAL_ALL);
}

void api_cleanup(void) {
    curl_global_cleanup();
#ifdef __3DS__
    if (soc_initialised) {
        socExit();
        soc_initialised = false;
    }
    if (soc_buffer) { free(soc_buffer); soc_buffer = NULL; }
#endif
}

/* ---------- Build JSON request body ---------- */

static char *build_request_body(const AppConfig *cfg, const Conversation *conv) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "model", cfg->model[0] ? cfg->model : DEFAULT_MODEL);
    cJSON_AddNumberToObject(root, "max_tokens", cfg->max_tokens > 0 ? cfg->max_tokens : DEFAULT_MAX_TOKENS);

    if (cfg->system_prompt[0]) {
        cJSON_AddStringToObject(root, "system", cfg->system_prompt);
    }

    cJSON *messages = cJSON_AddArrayToObject(root, "messages");
    for (int i = 0; i < conv->count; i++) {
        cJSON *msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "role",    chat_role_str(conv->messages[i].role));
        cJSON_AddStringToObject(msg, "content", conv->messages[i].content);
        cJSON_AddItemToArray(messages, msg);
    }

    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return body;
}

/* ---------- Parse assistant text from response JSON ---------- */

static int parse_response(const char *json_str, char *out, size_t out_len) {
    cJSON *root = cJSON_Parse(json_str);
    if (!root) return -1;

    /* Check for API error object: { "error": { "message": "..." } } */
    cJSON *error = cJSON_GetObjectItem(root, "error");
    if (error) {
        cJSON *msg = cJSON_GetObjectItem(error, "message");
        if (msg && msg->valuestring)
            snprintf(out, out_len, "API error: %s", msg->valuestring);
        else
            snprintf(out, out_len, "Unknown API error");
        cJSON_Delete(root);
        return -1;
    }

    /* Normal response: content[0].text */
    cJSON *content = cJSON_GetObjectItem(root, "content");
    if (!content) { cJSON_Delete(root); return -1; }
    cJSON *first = cJSON_GetArrayItem(content, 0);
    if (!first)   { cJSON_Delete(root); return -1; }
    cJSON *text   = cJSON_GetObjectItem(first, "text");
    if (!text || !text->valuestring) { cJSON_Delete(root); return -1; }

    strncpy(out, text->valuestring, out_len - 1);
    out[out_len - 1] = '\0';
    cJSON_Delete(root);
    return 0;
}

/* ---------- Main send function ---------- */

ApiResult api_send(const AppConfig *cfg, Conversation *conv,
                   char *out_error_msg, int err_len) {
    if (!cfg->api_key[0]) {
        if (out_error_msg) snprintf(out_error_msg, err_len, "API key not set. Go to Settings.");
        return API_ERR_NO_KEY;
    }

    char *body = build_request_body(cfg, conv);
    if (!body) {
        if (out_error_msg) snprintf(out_error_msg, err_len, "Memory allocation failed.");
        return API_ERR_MEMORY;
    }

    ResponseBuf resp;
    if (response_buf_init(&resp, 8192) != 0) {
        free(body);
        if (out_error_msg) snprintf(out_error_msg, err_len, "Memory allocation failed.");
        return API_ERR_MEMORY;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        free(body);
        response_buf_free(&resp);
        if (out_error_msg) snprintf(out_error_msg, err_len, "curl init failed.");
        return API_ERR_NETWORK;
    }

    /* Build headers */
    struct curl_slist *headers = NULL;
    char auth_header[API_KEY_LEN + 20];
    snprintf(auth_header, sizeof(auth_header), "x-api-key: %s", cfg->api_key);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "anthropic-version: " ANTHROPIC_VERSION);
    headers = curl_slist_append(headers, "content-type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL,            ANTHROPIC_API_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        (long)API_TIMEOUT_SECS);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    /* SSL: use devkitPro bundled CA bundle if available */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
#ifdef __3DS__
    curl_easy_setopt(curl, CURLOPT_CAINFO, "romfs:/certs/cacert.pem");
#endif

    CURLcode res = curl_easy_perform(curl);

    ApiResult result = API_OK;

    if (res != CURLE_OK) {
        if (out_error_msg)
            snprintf(out_error_msg, err_len, "Network error: %s", curl_easy_strerror(res));
        result = API_ERR_NETWORK;
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code == 401) {
            if (out_error_msg) snprintf(out_error_msg, err_len, "Invalid API key (HTTP 401).");
            result = API_ERR_AUTH;
        } else if (http_code == 429) {
            if (out_error_msg) snprintf(out_error_msg, err_len, "Rate limit exceeded (HTTP 429). Wait and retry.");
            result = API_ERR_RATE_LIMIT;
        } else if (http_code != 200) {
            if (out_error_msg)
                snprintf(out_error_msg, err_len, "HTTP error %ld.", http_code);
            result = API_ERR_HTTP;
        } else {
            /* Parse the assistant reply */
            char reply[MAX_MSG_LEN];
            reply[0] = '\0';
            if (parse_response(resp.data, reply, sizeof(reply)) != 0) {
                /* parse_response may have written an error message into reply */
                if (out_error_msg) {
                    if (reply[0])
                        snprintf(out_error_msg, err_len, "%s", reply);
                    else
                        snprintf(out_error_msg, err_len, "Error parsing API response.");
                }
                result = API_ERR_PARSE;
            } else {
                /* Success — add the assistant message to the conversation */
                chat_add_message(conv, ROLE_ASSISTANT, reply);
                result = API_OK;
            }
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(body);
    response_buf_free(&resp);
    return result;
}

const char *api_result_str(ApiResult r) {
    switch (r) {
        case API_OK:             return "OK";
        case API_ERR_NO_KEY:     return "API key not configured";
        case API_ERR_NETWORK:    return "Network error";
        case API_ERR_HTTP:       return "HTTP error";
        case API_ERR_PARSE:      return "JSON parse error";
        case API_ERR_RATE_LIMIT: return "Rate limit exceeded";
        case API_ERR_AUTH:       return "Authentication failed";
        case API_ERR_MEMORY:     return "Memory error";
        default:                 return "Unknown error";
    }
}
