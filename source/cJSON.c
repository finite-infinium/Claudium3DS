/*
  cJSON — Ultralightweight JSON parser in ANSI C
  Copyright (c) 2009-2017 Dave Gamble and cJSON contributors
  SPDX-License-Identifier: MIT

  Adapted / trimmed for the Claudium3DS 3DS homebrew project.
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <locale.h>

#include "cJSON.h"

#define cJSON_VERSION_MAJOR 1
#define cJSON_VERSION_MINOR 7
#define cJSON_VERSION_PATCH 16

static const char* const cJSON_version = "1.7.16";
CJSON_PUBLIC(const char*) cJSON_Version(void) { return cJSON_version; }

/* ---- memory hooks ---- */
static void* internal_malloc(size_t size)   { return malloc(size); }
static void  internal_free(void *pointer)   { free(pointer); }
static void* internal_realloc(void *p, size_t size) { return realloc(p, size); }

static void*(CJSON_CDECL *cJSON_malloc)(size_t size) = internal_malloc;
static void (CJSON_CDECL *cJSON_free)(void *object)  = internal_free;

CJSON_PUBLIC(void) cJSON_InitHooks(cJSON_Hooks *hooks) {
    if (!hooks) {
        cJSON_malloc = internal_malloc;
        cJSON_free   = internal_free;
        return;
    }
    cJSON_malloc = hooks->malloc_fn ? hooks->malloc_fn : internal_malloc;
    cJSON_free   = hooks->free_fn   ? hooks->free_fn   : internal_free;
}

/* ---- error tracking ---- */
static const char *global_ep = NULL;
CJSON_PUBLIC(const char*) cJSON_GetErrorPtr(void) { return global_ep; }

/* ---- helpers ---- */
static unsigned char*    cast_uchar(const char *string)  { return (unsigned char*)string; }
static char*             cast_char(const unsigned char *string) { return (char*)string; }

static cJSON* cJSON_New_Item(void) {
    cJSON *node = (cJSON*)cJSON_malloc(sizeof(cJSON));
    if (node) memset(node, 0, sizeof(cJSON));
    return node;
}

CJSON_PUBLIC(void) cJSON_Delete(cJSON *item) {
    cJSON *next;
    while (item) {
        next = item->next;
        if (!(item->type & cJSON_IsReference) && item->child) cJSON_Delete(item->child);
        if (!(item->type & cJSON_IsReference) && item->valuestring) cJSON_free(item->valuestring);
        if (!(item->type & cJSON_StringIsConst) && item->string)    cJSON_free(item->string);
        cJSON_free(item);
        item = next;
    }
}

/* parse 4-hex-digit unicode */
static unsigned parse_hex4(const unsigned char *str) {
    unsigned h = 0;
    int i;
    for (i = 0; i < 4; i++) {
        unsigned char c = str[i];
        if      (c >= '0' && c <= '9') h = (h << 4) + (c - '0');
        else if (c >= 'A' && c <= 'F') h = (h << 4) + (10 + c - 'A');
        else if (c >= 'a' && c <= 'f') h = (h << 4) + (10 + c - 'a');
        else return 0;
    }
    return h;
}

/* encode to UTF-8 */
static unsigned char* encode_utf8(unsigned char *out, unsigned codepoint) {
    if (codepoint <= 0x7F) {
        *out++ = (unsigned char)codepoint;
    } else if (codepoint <= 0x7FF) {
        *out++ = (unsigned char)(0xC0 | (codepoint >> 6));
        *out++ = (unsigned char)(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        *out++ = (unsigned char)(0xE0 | (codepoint >> 12));
        *out++ = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
        *out++ = (unsigned char)(0x80 | (codepoint & 0x3F));
    } else {
        *out++ = (unsigned char)(0xF0 | (codepoint >> 18));
        *out++ = (unsigned char)(0x80 | ((codepoint >> 12) & 0x3F));
        *out++ = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
        *out++ = (unsigned char)(0x80 | (codepoint & 0x3F));
    }
    return out;
}

/* Parse a string from the input */
static const unsigned char* parse_string(cJSON *item, const unsigned char *str, const unsigned char **ep) {
    const unsigned char *ptr = str + 1;
    const unsigned char *ptr2;
    unsigned char *out;
    int len = 0;
    unsigned uc, uc2;

    if (*str != '\"') { *ep = str; return NULL; }

    /* count characters needed */
    while (*ptr != '\"' && *ptr) {
        if (*ptr++ == '\\') ptr++;
        len++;
    }

    out = (unsigned char*)cJSON_malloc(len + 1);
    if (!out) return NULL;

    ptr = str + 1;
    ptr2 = out;
    while (*ptr != '\"' && *ptr) {
        if (*ptr != '\\') {
            *((unsigned char*)ptr2++) = *ptr++;
        } else {
            ptr++;
            switch (*ptr) {
                case 'b': *((unsigned char*)ptr2++) = '\b'; break;
                case 'f': *((unsigned char*)ptr2++) = '\f'; break;
                case 'n': *((unsigned char*)ptr2++) = '\n'; break;
                case 'r': *((unsigned char*)ptr2++) = '\r'; break;
                case 't': *((unsigned char*)ptr2++) = '\t'; break;
                case 'u':
                    uc = parse_hex4(ptr + 1);
                    ptr += 4;
                    if ((uc >= 0xDC00 && uc <= 0xDFFF)) { cJSON_free(out); return NULL; }
                    if (uc >= 0xD800 && uc <= 0xDBFF) {
                        if (ptr[1] != '\\' || ptr[2] != 'u') { cJSON_free(out); return NULL; }
                        uc2 = parse_hex4(ptr + 3);
                        ptr += 6;
                        if (uc2 < 0xDC00 || uc2 > 0xDFFF) { cJSON_free(out); return NULL; }
                        uc = 0x10000 + (((uc & 0x3FF) << 10) | (uc2 & 0x3FF));
                    }
                    ptr2 = encode_utf8((unsigned char*)ptr2, uc);
                    break;
                default:
                    *((unsigned char*)ptr2++) = *ptr;
                    break;
            }
            ptr++;
        }
    }
    *((unsigned char*)ptr2) = '\0';
    if (*ptr == '\"') ptr++;
    item->valuestring = cast_char(out);
    item->type = cJSON_String;
    return ptr;
}

/* Print a string (with escaping) */
static char* print_string_ptr(const unsigned char *str) {
    const unsigned char *ptr;
    unsigned char *out;
    unsigned char *ptr2;
    int len = 0;
    unsigned char token;

    if (!str) return (char*)cJSON_malloc(3); /* "" */

    for (ptr = str; *ptr; ptr++) {
        unsigned char c = *ptr;
        if (c == '\"' || c == '\\' || c < 32) len += 2;
        else len++;
    }

    out = (unsigned char*)cJSON_malloc(len + 3);
    if (!out) return NULL;

    ptr2 = out;
    *ptr2++ = '\"';
    for (ptr = str; *ptr; ptr++) {
        if ((unsigned char)*ptr > 31 && *ptr != '\"' && *ptr != '\\') {
            *ptr2++ = *ptr;
        } else {
            *ptr2++ = '\\';
            switch (token = *ptr) {
                case '\\': *ptr2++ = '\\'; break;
                case '\"': *ptr2++ = '\"'; break;
                case '\b': *ptr2++ = 'b';  break;
                case '\f': *ptr2++ = 'f';  break;
                case '\n': *ptr2++ = 'n';  break;
                case '\r': *ptr2++ = 'r';  break;
                case '\t': *ptr2++ = 't';  break;
                default:
                    sprintf((char*)ptr2, "u%04X", token);
                    ptr2 += 5;
                    break;
            }
        }
    }
    *ptr2++ = '\"';
    *ptr2   = '\0';
    return cast_char(out);
}

static char* print_string(const cJSON *item) {
    return print_string_ptr(cast_uchar(item->valuestring));
}

/* Skip whitespace */
static const unsigned char* skip(const unsigned char *in) {
    while (in && *in && (unsigned char)*in <= 32) in++;
    return in;
}

/* Forward declarations */
static const unsigned char* parse_value(cJSON *item, const unsigned char *value, const unsigned char **ep);
static char* print_value(const cJSON *item, int depth, cJSON_bool fmt);
static const unsigned char* parse_array(cJSON *item, const unsigned char *value, const unsigned char **ep);
static char* print_array(const cJSON *item, int depth, cJSON_bool fmt);
static const unsigned char* parse_object(cJSON *item, const unsigned char *value, const unsigned char **ep);
static char* print_object(const cJSON *item, int depth, cJSON_bool fmt);

/* Parse a number */
static const unsigned char* parse_number(cJSON *item, const unsigned char *num) {
    double n = 0, sign = 1, scale = 0;
    int subscale = 0, signsubscale = 1;

    if (*num == '-') { sign = -1; num++; }
    if (*num == '0') num++;
    if (*num >= '1' && *num <= '9') {
        do { n = (n * 10.0) + (*num++ - '0'); } while (*num >= '0' && *num <= '9');
    }
    if (*num == '.') {
        num++;
        do { n = (n * 10.0) + (*num++ - '0'); scale--; } while (*num >= '0' && *num <= '9');
    }
    if (*num == 'e' || *num == 'E') {
        num++;
        if (*num == '+') num++;
        else if (*num == '-') { signsubscale = -1; num++; }
        while (*num >= '0' && *num <= '9') subscale = subscale * 10 + (*num++ - '0');
    }
    n = sign * n * pow(10.0, scale + subscale * signsubscale);
    item->valuedouble = n;
    item->valueint = (int)n;
    item->type = cJSON_Number;
    return num;
}

/* Print a number */
static char* print_number(const cJSON *item) {
    char *str;
    double d = item->valuedouble;
    if (fabs(((double)item->valueint) - d) <= DBL_EPSILON && d <= INT_MAX && d >= INT_MIN) {
        str = (char*)cJSON_malloc(21);
        if (str) sprintf(str, "%d", item->valueint);
    } else {
        str = (char*)cJSON_malloc(64);
        if (str) {
            if (fabs(floor(d) - d) <= DBL_EPSILON && fabs(d) < 1.0e60)
                sprintf(str, "%.0f", d);
            else if (fabs(d) < 1.0e-6 || fabs(d) > 1.0e9)
                sprintf(str, "%e", d);
            else
                sprintf(str, "%f", d);
        }
    }
    return str;
}

static const unsigned char* parse_value(cJSON *item, const unsigned char *value, const unsigned char **ep) {
    if (!value) return NULL;
    if (!strncmp((const char*)value, "null",  4)) { item->type = cJSON_NULL;  return value + 4; }
    if (!strncmp((const char*)value, "false", 5)) { item->type = cJSON_False; return value + 5; }
    if (!strncmp((const char*)value, "true",  4)) { item->type = cJSON_True; item->valueint = 1; return value + 4; }
    if (*value == '\"') return parse_string(item, value, ep);
    if (*value == '-' || (*value >= '0' && *value <= '9')) return parse_number(item, value);
    if (*value == '[') return parse_array(item, value, ep);
    if (*value == '{') return parse_object(item, value, ep);
    *ep = value;
    return NULL;
}

static char* print_value(const cJSON *item, int depth, cJSON_bool fmt) {
    char *out = NULL;
    if (!item) return NULL;
    switch ((item->type) & 0xFF) {
        case cJSON_NULL:   out = (char*)cJSON_malloc(5); if (out) strcpy(out, "null");  break;
        case cJSON_False:  out = (char*)cJSON_malloc(6); if (out) strcpy(out, "false"); break;
        case cJSON_True:   out = (char*)cJSON_malloc(5); if (out) strcpy(out, "true");  break;
        case cJSON_Number: out = print_number(item); break;
        case cJSON_Raw:    out = (char*)cJSON_malloc(strlen(item->valuestring) + 1); if (out) strcpy(out, item->valuestring); break;
        case cJSON_String: out = print_string(item); break;
        case cJSON_Array:  out = print_array(item, depth, fmt); break;
        case cJSON_Object: out = print_object(item, depth, fmt); break;
    }
    return out;
}

static const unsigned char* parse_array(cJSON *item, const unsigned char *value, const unsigned char **ep) {
    cJSON *child;
    if (*value != '[') { *ep = value; return NULL; }
    item->type = cJSON_Array;
    value = skip(value + 1);
    if (*value == ']') return value + 1;
    item->child = child = cJSON_New_Item();
    if (!item->child) return NULL;
    value = skip(parse_value(child, skip(value), ep));
    if (!value) return NULL;
    while (*value == ',') {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) return NULL;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip(parse_value(child, skip(value + 1), ep));
        if (!value) return NULL;
    }
    if (*value == ']') return value + 1;
    *ep = value;
    return NULL;
}

static char* print_array(const cJSON *item, int depth, cJSON_bool fmt) {
    char **entries;
    char *out = NULL;
    char *ptr, *ret;
    int len = 5, i = 0, fail = 0;
    size_t tmplen = 0;
    cJSON *child = item->child;
    int numentries = 0;
    while (child) { numentries++; child = child->next; }
    if (!numentries) {
        out = (char*)cJSON_malloc(3);
        if (out) strcpy(out, "[]");
        return out;
    }
    entries = (char**)cJSON_malloc(numentries * sizeof(char*));
    if (!entries) return NULL;
    memset(entries, 0, numentries * sizeof(char*));
    child = item->child;
    while (child && !fail) {
        ret = print_value(child, depth + 1, fmt);
        entries[i++] = ret;
        if (ret) len += strlen(ret) + 2 + (fmt ? 1 : 0);
        else fail = 1;
        child = child->next;
    }
    if (!fail) {
        out = (char*)cJSON_malloc(len);
        if (!out) fail = 1;
    }
    if (fail) {
        for (i = 0; i < numentries; i++) if (entries[i]) cJSON_free(entries[i]);
        cJSON_free(entries);
        return NULL;
    }
    *out = '[';
    ptr = out + 1;
    *ptr = '\0';
    for (i = 0; i < numentries; i++) {
        tmplen = strlen(entries[i]);
        memcpy(ptr, entries[i], tmplen);
        ptr += tmplen;
        if (i != numentries - 1) { *ptr++ = ','; if (fmt) *ptr++ = ' '; *ptr = '\0'; }
        cJSON_free(entries[i]);
    }
    cJSON_free(entries);
    *ptr++ = ']';
    *ptr   = '\0';
    return out;
}

static const unsigned char* parse_object(cJSON *item, const unsigned char *value, const unsigned char **ep) {
    cJSON *child;
    if (*value != '{') { *ep = value; return NULL; }
    item->type = cJSON_Object;
    value = skip(value + 1);
    if (*value == '}') return value + 1;
    item->child = child = cJSON_New_Item();
    if (!item->child) return NULL;
    value = skip(parse_string(child, skip(value), ep));
    if (!value) return NULL;
    child->string = child->valuestring;
    child->valuestring = NULL;
    if (*value != ':') { *ep = value; return NULL; }
    value = skip(parse_value(child, skip(value + 1), ep));
    if (!value) return NULL;
    while (*value == ',') {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) return NULL;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip(parse_string(child, skip(value + 1), ep));
        if (!value) return NULL;
        child->string = child->valuestring;
        child->valuestring = NULL;
        if (*value != ':') { *ep = value; return NULL; }
        value = skip(parse_value(child, skip(value + 1), ep));
        if (!value) return NULL;
    }
    if (*value == '}') return value + 1;
    *ep = value;
    return NULL;
}

static char* print_object(const cJSON *item, int depth, cJSON_bool fmt) {
    char **entries = NULL, **names = NULL;
    char *out = NULL, *ptr, *ret, *str;
    int len = 7, i = 0, j, fail = 0;
    size_t tmplen = 0;
    cJSON *child = item->child;
    int numentries = 0;
    while (child) { numentries++; child = child->next; }
    if (!numentries) {
        out = (char*)cJSON_malloc(fmt ? depth + 4 : 3);
        if (!out) return NULL;
        ptr = out;
        *ptr++ = '{';
        if (fmt) { *ptr++ = '\n'; for (j = 0; j < depth - 1; j++) *ptr++ = '\t'; }
        *ptr++ = '}';
        *ptr   = '\0';
        return out;
    }
    entries = (char**)cJSON_malloc(numentries * sizeof(char*));
    names   = (char**)cJSON_malloc(numentries * sizeof(char*));
    if (!entries || !names) { cJSON_free(entries); cJSON_free(names); return NULL; }
    memset(entries, 0, numentries * sizeof(char*));
    memset(names,   0, numentries * sizeof(char*));
    child = item->child;
    depth++;
    if (fmt) len += depth;
    while (child && !fail) {
        names[i] = str = print_string_ptr(cast_uchar(child->string));
        entries[i++] = ret = print_value(child, depth, fmt);
        if (str && ret) len += strlen(ret) + strlen(str) + 2 + (fmt ? 2 + depth + 1 : 0);
        else fail = 1;
        child = child->next;
    }
    if (!fail) { out = (char*)cJSON_malloc(len); if (!out) fail = 1; }
    if (fail) {
        for (i = 0; i < numentries; i++) { if (names[i]) cJSON_free(names[i]); if (entries[i]) cJSON_free(entries[i]); }
        cJSON_free(names); cJSON_free(entries);
        return NULL;
    }
    *out = '{'; ptr = out + 1;
    if (fmt) *ptr++ = '\n';
    *ptr = '\0';
    for (i = 0; i < numentries; i++) {
        if (fmt) for (j = 0; j < depth; j++) *ptr++ = '\t';
        tmplen = strlen(names[i]);
        memcpy(ptr, names[i], tmplen); ptr += tmplen;
        *ptr++ = ':'; if (fmt) *ptr++ = ' ';
        tmplen = strlen(entries[i]);
        memcpy(ptr, entries[i], tmplen); ptr += tmplen;
        if (i != numentries - 1) *ptr++ = ',';
        if (fmt) *ptr++ = '\n';
        *ptr = '\0';
        cJSON_free(names[i]); cJSON_free(entries[i]);
    }
    cJSON_free(names); cJSON_free(entries);
    if (fmt) for (j = 0; j < depth - 1; j++) *ptr++ = '\t';
    *ptr++ = '}'; *ptr = '\0';
    return out;
}

/* Public parse/print API */
CJSON_PUBLIC(cJSON*) cJSON_ParseWithLength(const char *value, size_t buffer_length) {
    cJSON *c;
    const unsigned char *ep = NULL;
    if (!value || buffer_length == 0) return NULL;
    c = cJSON_New_Item();
    if (!c) return NULL;
    if (!parse_value(c, skip(cast_uchar(value)), &ep)) {
        global_ep = (const char*)ep;
        cJSON_Delete(c);
        return NULL;
    }
    return c;
}

CJSON_PUBLIC(cJSON*) cJSON_Parse(const char *value) {
    return value ? cJSON_ParseWithLength(value, strlen(value) + 1) : NULL;
}

CJSON_PUBLIC(char*) cJSON_Print(const cJSON *item) {
    return print_value(item, 0, 1);
}

CJSON_PUBLIC(char*) cJSON_PrintUnformatted(const cJSON *item) {
    return print_value(item, 0, 0);
}

CJSON_PUBLIC(char*) cJSON_PrintBuffered(const cJSON *item, int prebuffer, cJSON_bool fmt) {
    (void)prebuffer;
    return print_value(item, 0, fmt);
}

CJSON_PUBLIC(cJSON_bool) cJSON_PrintPreallocated(cJSON *item, char *buf, const int len, const cJSON_bool fmt) {
    char *out = print_value(item, 0, fmt);
    if (!out) return 0;
    if ((int)strlen(out) >= len) { cJSON_free(out); return 0; }
    strcpy(buf, out);
    cJSON_free(out);
    return 1;
}

/* Traversal */
CJSON_PUBLIC(int) cJSON_GetArraySize(const cJSON *array) {
    cJSON *c = array ? array->child : NULL;
    int i = 0;
    while (c) { i++; c = c->next; }
    return i;
}

CJSON_PUBLIC(cJSON*) cJSON_GetArrayItem(const cJSON *array, int item) {
    cJSON *c = array ? array->child : NULL;
    while (c && item > 0) { item--; c = c->next; }
    return c;
}

CJSON_PUBLIC(cJSON*) cJSON_GetObjectItem(const cJSON * const object, const char * const string) {
    cJSON *c = object ? object->child : NULL;
    while (c && strcasecmp(c->string, string)) c = c->next;
    return c;
}

CJSON_PUBLIC(cJSON*) cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string) {
    cJSON *c = object ? object->child : NULL;
    while (c && strcmp(c->string, string)) c = c->next;
    return c;
}

CJSON_PUBLIC(cJSON_bool) cJSON_HasObjectItem(const cJSON *object, const char *string) {
    return cJSON_GetObjectItem(object, string) ? 1 : 0;
}

/* Creation helpers */
CJSON_PUBLIC(cJSON*) cJSON_CreateNull(void)  { cJSON *i = cJSON_New_Item(); if (i) i->type = cJSON_NULL;  return i; }
CJSON_PUBLIC(cJSON*) cJSON_CreateTrue(void)  { cJSON *i = cJSON_New_Item(); if (i) { i->type = cJSON_True; i->valueint = 1; } return i; }
CJSON_PUBLIC(cJSON*) cJSON_CreateFalse(void) { cJSON *i = cJSON_New_Item(); if (i) i->type = cJSON_False; return i; }
CJSON_PUBLIC(cJSON*) cJSON_CreateBool(cJSON_bool b) { return b ? cJSON_CreateTrue() : cJSON_CreateFalse(); }

CJSON_PUBLIC(cJSON*) cJSON_CreateNumber(double num) {
    cJSON *i = cJSON_New_Item();
    if (i) { i->type = cJSON_Number; i->valuedouble = num; i->valueint = (int)num; }
    return i;
}

CJSON_PUBLIC(cJSON*) cJSON_CreateString(const char *string) {
    cJSON *i = cJSON_New_Item();
    if (i) {
        i->type = cJSON_String;
        i->valuestring = (char*)cJSON_malloc(strlen(string) + 1);
        if (!i->valuestring) { cJSON_free(i); return NULL; }
        strcpy(i->valuestring, string);
    }
    return i;
}

CJSON_PUBLIC(cJSON*) cJSON_CreateRaw(const char *raw) {
    cJSON *i = cJSON_New_Item();
    if (i) {
        i->type = cJSON_Raw;
        i->valuestring = (char*)cJSON_malloc(strlen(raw) + 1);
        if (!i->valuestring) { cJSON_free(i); return NULL; }
        strcpy(i->valuestring, raw);
    }
    return i;
}

CJSON_PUBLIC(cJSON*) cJSON_CreateArray(void)  { cJSON *i = cJSON_New_Item(); if (i) i->type = cJSON_Array;  return i; }
CJSON_PUBLIC(cJSON*) cJSON_CreateObject(void) { cJSON *i = cJSON_New_Item(); if (i) i->type = cJSON_Object; return i; }

/* Add item helpers */
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToArray(cJSON *array, cJSON *item) {
    cJSON *c;
    if (!array || !item) return 0;
    c = array->child;
    if (!c) { array->child = item; return 1; }
    while (c && c->next) c = c->next;
    c->next = item;
    item->prev = c;
    return 1;
}

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item) {
    if (!object || !item || !string) return 0;
    if (item->string) cJSON_free(item->string);
    item->string = (char*)cJSON_malloc(strlen(string) + 1);
    if (!item->string) return 0;
    strcpy(item->string, string);
    return cJSON_AddItemToArray(object, item);
}

/* Add*ToObject helpers */
CJSON_PUBLIC(cJSON*) cJSON_AddNullToObject(cJSON * const object, const char * const name)
    { cJSON *n = cJSON_CreateNull();   cJSON_AddItemToObject(object, name, n); return n; }
CJSON_PUBLIC(cJSON*) cJSON_AddTrueToObject(cJSON * const object, const char * const name)
    { cJSON *n = cJSON_CreateTrue();   cJSON_AddItemToObject(object, name, n); return n; }
CJSON_PUBLIC(cJSON*) cJSON_AddFalseToObject(cJSON * const object, const char * const name)
    { cJSON *n = cJSON_CreateFalse();  cJSON_AddItemToObject(object, name, n); return n; }
CJSON_PUBLIC(cJSON*) cJSON_AddBoolToObject(cJSON * const object, const char * const name, const cJSON_bool boolean)
    { cJSON *n = cJSON_CreateBool(boolean); cJSON_AddItemToObject(object, name, n); return n; }
CJSON_PUBLIC(cJSON*) cJSON_AddNumberToObject(cJSON * const object, const char * const name, const double number)
    { cJSON *n = cJSON_CreateNumber(number); cJSON_AddItemToObject(object, name, n); return n; }
CJSON_PUBLIC(cJSON*) cJSON_AddStringToObject(cJSON * const object, const char * const name, const char * const string)
    { cJSON *n = cJSON_CreateString(string); cJSON_AddItemToObject(object, name, n); return n; }
CJSON_PUBLIC(cJSON*) cJSON_AddRawToObject(cJSON * const object, const char * const name, const char * const raw)
    { cJSON *n = cJSON_CreateRaw(raw); cJSON_AddItemToObject(object, name, n); return n; }
CJSON_PUBLIC(cJSON*) cJSON_AddObjectToObject(cJSON * const object, const char * const name)
    { cJSON *n = cJSON_CreateObject(); cJSON_AddItemToObject(object, name, n); return n; }
CJSON_PUBLIC(cJSON*) cJSON_AddArrayToObject(cJSON * const object, const char * const name)
    { cJSON *n = cJSON_CreateArray();  cJSON_AddItemToObject(object, name, n); return n; }

/* Duplicate */
CJSON_PUBLIC(cJSON*) cJSON_Duplicate(const cJSON *item, cJSON_bool recurse) {
    cJSON *newitem, *cptr, *nptr = NULL, *newchild;
    if (!item) return NULL;
    newitem = cJSON_New_Item();
    if (!newitem) return NULL;
    newitem->type = item->type & (~cJSON_IsReference);
    newitem->valueint = item->valueint;
    newitem->valuedouble = item->valuedouble;
    if (item->valuestring) {
        newitem->valuestring = (char*)cJSON_malloc(strlen(item->valuestring) + 1);
        if (!newitem->valuestring) { cJSON_Delete(newitem); return NULL; }
        strcpy(newitem->valuestring, item->valuestring);
    }
    if (item->string) {
        newitem->string = (char*)cJSON_malloc(strlen(item->string) + 1);
        if (!newitem->string) { cJSON_Delete(newitem); return NULL; }
        strcpy(newitem->string, item->string);
    }
    if (!recurse) return newitem;
    cptr = item->child;
    while (cptr) {
        newchild = cJSON_Duplicate(cptr, 1);
        if (!newchild) { cJSON_Delete(newitem); return NULL; }
        if (nptr) { nptr->next = newchild; newchild->prev = nptr; nptr = newchild; }
        else { newitem->child = newchild; nptr = newchild; }
        cptr = cptr->next;
    }
    return newitem;
}

/* Compare */
CJSON_PUBLIC(cJSON_bool) cJSON_Compare(const cJSON * const a, const cJSON * const b, const cJSON_bool case_sensitive) {
    if (!a || !b) return 0;
    if ((a->type & 0xFF) != (b->type & 0xFF)) return 0;
    switch (a->type & 0xFF) {
        case cJSON_NULL:  case cJSON_False: case cJSON_True: return 1;
        case cJSON_Number: return (fabs(a->valuedouble - b->valuedouble) <= DBL_EPSILON) ? 1 : 0;
        case cJSON_String: case cJSON_Raw:
            if (!a->valuestring || !b->valuestring) return 0;
            return (case_sensitive ? strcmp(a->valuestring, b->valuestring) : strcasecmp(a->valuestring, b->valuestring)) == 0 ? 1 : 0;
        default: return 0;
    }
}

/* Minify */
CJSON_PUBLIC(void) cJSON_Minify(char *json) {
    unsigned char *into = cast_uchar(json);
    while (*json) {
        if (*json == ' ' || *json == '\t' || *json == '\r' || *json == '\n') json++;
        else if (*json == '/') {
            if (*(json + 1) == '/') { while (*json && *json != '\n') json++; }
            else if (*(json + 1) == '*') { while (*json && !(*json == '*' && *(json + 1) == '/')) json++; json += 2; }
            else *into++ = (unsigned char)*json++;
        }
        else if (*json == '\"') {
            *into++ = (unsigned char)*json++;
            while (*json && *json != '\"') { if (*json == '\\') *into++ = (unsigned char)*json++; *into++ = (unsigned char)*json++; }
            *into++ = (unsigned char)*json++;
        }
        else *into++ = (unsigned char)*json++;
    }
    *into = '\0';
}

#ifdef __cplusplus
}
#endif
