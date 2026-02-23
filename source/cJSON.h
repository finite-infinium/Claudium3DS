/*
  cJSON — Ultralightweight JSON parser in ANSI C
  Copyright (c) 2009-2017 Dave Gamble and cJSON contributors
  SPDX-License-Identifier: MIT
*/

#ifndef cJSON__h
#define cJSON__h

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* Boolean type — must come first so subsequent declarations can use it */
#ifndef cJSON_bool
typedef int cJSON_bool;
#endif

/* cJSON Types */
#define cJSON_Invalid  (0)
#define cJSON_False    (1 << 0)
#define cJSON_True     (1 << 1)
#define cJSON_NULL     (1 << 2)
#define cJSON_Number   (1 << 3)
#define cJSON_String   (1 << 4)
#define cJSON_Array    (1 << 5)
#define cJSON_Object   (1 << 6)
#define cJSON_Raw      (1 << 7)

#define cJSON_IsReference   256
#define cJSON_StringIsConst 512

typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    int type;
    char *valuestring;
    int valueint;
    double valuedouble;
    char *string;
} cJSON;

#ifndef CJSON_CDECL
#define CJSON_CDECL
#endif

#ifndef CJSON_PUBLIC
#define CJSON_PUBLIC(type) type CJSON_CDECL
#endif

typedef struct cJSON_Hooks {
    void *(*malloc_fn)(size_t sz);
    void  (*free_fn)(void *ptr);
} cJSON_Hooks;

/* Supply hooks for memory allocation */
CJSON_PUBLIC(void)   cJSON_InitHooks(cJSON_Hooks *hooks);

/* Parsing */
CJSON_PUBLIC(cJSON*) cJSON_Parse(const char *value);
CJSON_PUBLIC(cJSON*) cJSON_ParseWithLength(const char *value, size_t buffer_length);

/* Printing */
CJSON_PUBLIC(char*)  cJSON_Print(const cJSON *item);
CJSON_PUBLIC(char*)  cJSON_PrintUnformatted(const cJSON *item);
CJSON_PUBLIC(char*)  cJSON_PrintBuffered(const cJSON *item, int prebuffer, cJSON_bool fmt);
CJSON_PUBLIC(cJSON_bool) cJSON_PrintPreallocated(cJSON *item, char *buffer, const int length, const cJSON_bool format);

/* Delete cJSON tree */
CJSON_PUBLIC(void)   cJSON_Delete(cJSON *item);

/* Traversal */
CJSON_PUBLIC(int)    cJSON_GetArraySize(const cJSON *array);
CJSON_PUBLIC(cJSON*) cJSON_GetArrayItem(const cJSON *array, int index);
CJSON_PUBLIC(cJSON*) cJSON_GetObjectItem(const cJSON * const object, const char * const string);
CJSON_PUBLIC(cJSON*) cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string);
CJSON_PUBLIC(cJSON_bool) cJSON_HasObjectItem(const cJSON *object, const char *string);

/* Useful macros */
#define cJSON_ArrayForEach(element, array) \
    for(element = (array != NULL) ? (array)->child : NULL; element != NULL; element = element->next)

/* Creation helpers */
CJSON_PUBLIC(cJSON*) cJSON_CreateNull(void);
CJSON_PUBLIC(cJSON*) cJSON_CreateTrue(void);
CJSON_PUBLIC(cJSON*) cJSON_CreateFalse(void);
CJSON_PUBLIC(cJSON*) cJSON_CreateBool(cJSON_bool boolean);
CJSON_PUBLIC(cJSON*) cJSON_CreateNumber(double num);
CJSON_PUBLIC(cJSON*) cJSON_CreateString(const char *string);
CJSON_PUBLIC(cJSON*) cJSON_CreateRaw(const char *raw);
CJSON_PUBLIC(cJSON*) cJSON_CreateArray(void);
CJSON_PUBLIC(cJSON*) cJSON_CreateObject(void);

/* Add item */
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToArray(cJSON *array, cJSON *item);
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);

/* Helpers to add items with no fuss */
CJSON_PUBLIC(cJSON*) cJSON_AddNullToObject(cJSON * const object, const char * const name);
CJSON_PUBLIC(cJSON*) cJSON_AddTrueToObject(cJSON * const object, const char * const name);
CJSON_PUBLIC(cJSON*) cJSON_AddFalseToObject(cJSON * const object, const char * const name);
CJSON_PUBLIC(cJSON*) cJSON_AddBoolToObject(cJSON * const object, const char * const name, const cJSON_bool boolean);
CJSON_PUBLIC(cJSON*) cJSON_AddNumberToObject(cJSON * const object, const char * const name, const double number);
CJSON_PUBLIC(cJSON*) cJSON_AddStringToObject(cJSON * const object, const char * const name, const char * const string);
CJSON_PUBLIC(cJSON*) cJSON_AddRawToObject(cJSON * const object, const char * const name, const char * const raw);
CJSON_PUBLIC(cJSON*) cJSON_AddObjectToObject(cJSON * const object, const char * const name);
CJSON_PUBLIC(cJSON*) cJSON_AddArrayToObject(cJSON * const object, const char * const name);

/* Type-checking helpers */
static inline cJSON_bool cJSON_IsInvalid(const cJSON * const item) { return item ? (item->type & 0xFF) == cJSON_Invalid : 0; }
static inline cJSON_bool cJSON_IsFalse(const cJSON * const item)   { return item ? (item->type & 0xFF) == cJSON_False   : 0; }
static inline cJSON_bool cJSON_IsTrue(const cJSON * const item)    { return item ? (item->type & 0xFF) == cJSON_True    : 0; }
static inline cJSON_bool cJSON_IsBool(const cJSON * const item)    { return item ? ((item->type & 0xFF) == cJSON_True || (item->type & 0xFF) == cJSON_False) : 0; }
static inline cJSON_bool cJSON_IsNull(const cJSON * const item)    { return item ? (item->type & 0xFF) == cJSON_NULL    : 0; }
static inline cJSON_bool cJSON_IsNumber(const cJSON * const item)  { return item ? (item->type & 0xFF) == cJSON_Number  : 0; }
static inline cJSON_bool cJSON_IsString(const cJSON * const item)  { return item ? (item->type & 0xFF) == cJSON_String  : 0; }
static inline cJSON_bool cJSON_IsArray(const cJSON * const item)   { return item ? (item->type & 0xFF) == cJSON_Array   : 0; }
static inline cJSON_bool cJSON_IsObject(const cJSON * const item)  { return item ? (item->type & 0xFF) == cJSON_Object  : 0; }
static inline cJSON_bool cJSON_IsRaw(const cJSON * const item)     { return item ? (item->type & 0xFF) == cJSON_Raw     : 0; }

/* Get error position */
CJSON_PUBLIC(const char*) cJSON_GetErrorPtr(void);

/* Duplicate */
CJSON_PUBLIC(cJSON*) cJSON_Duplicate(const cJSON *item, cJSON_bool recurse);

/* Compare */
CJSON_PUBLIC(cJSON_bool) cJSON_Compare(const cJSON * const a, const cJSON * const b, const cJSON_bool case_sensitive);

/* Minify (edit in place) */
CJSON_PUBLIC(void) cJSON_Minify(char *json);

/* Version */
CJSON_PUBLIC(const char*) cJSON_Version(void);

#ifdef __cplusplus
}
#endif

#endif /* cJSON__h */
