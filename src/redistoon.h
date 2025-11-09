#ifndef REDISTOON_H
#define REDISTOON_H

#include "redismodule.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

// Version information
#define REDISTOON_VERSION "0.1.0"
#define REDISTOON_MODULE_NAME "redisTOON"

// TOON value types
typedef enum {
    TOON_NULL,
    TOON_BOOLEAN,
    TOON_NUMBER,
    TOON_STRING,
    TOON_ARRAY,
    TOON_OBJECT,
    TOON_TABULAR_ARRAY  // Special type for TOON tabular arrays
} ToonType;

// Forward declarations
typedef struct ToonValue ToonValue;

// TOON object entry (key-value pair)
typedef struct {
    char *key;
    ToonValue *value;
} ToonObjectEntry;

// TOON tabular array structure
typedef struct {
    char **headers;     // Column headers
    size_t num_headers; // Number of columns
    ToonValue ***rows;  // Array of rows, each row is an array of values
    size_t num_rows;    // Number of rows
} ToonTabularArray;

// TOON value structure
struct ToonValue {
    ToonType type;
    union {
        bool boolean;
        double number;
        char *string;
        struct {
            ToonValue **elements;
            size_t length;
        } array;
        struct {
            ToonObjectEntry *entries;
            size_t length;
        } object;
        ToonTabularArray tabular;
    } value;
};

// Redis data type for TOON
typedef struct {
    ToonValue *root;
} ToonDocument;

// Function declarations

// Memory management
ToonValue *toon_value_create(ToonType type);
void toon_value_free(ToonValue *value);
ToonDocument *toon_document_create(void);
void toon_document_free(ToonDocument *doc);

// Encoding/Decoding
char *toon_encode(ToonValue *value, int indent_level);
ToonValue *toon_decode(const char *toon_string, char **error);

// JSON conversion
char *toon_to_json(ToonValue *value);
ToonValue *json_to_toon(const char *json_string, char **error);

// Path operations
ToonValue *toon_path_get(ToonValue *root, const char *path);
int toon_path_set(ToonValue *root, const char *path, ToonValue *value);
int toon_path_delete(ToonValue *root, const char *path);

// Utility functions
const char *toon_type_string(ToonType type);
size_t toon_estimate_tokens(ToonValue *value);

// Redis Module Type
extern RedisModuleType *ToonType_RMT;

// Redis type methods
void *ToonTypeRdbLoad(RedisModuleIO *rdb, int encver);
void ToonTypeRdbSave(RedisModuleIO *rdb, void *value);
void ToonTypeAofRewrite(RedisModuleIO *aof, RedisModuleString *key, void *value);
void ToonTypeDigest(RedisModuleDigest *md, void *value);
void ToonTypeFree(void *value);

#endif // REDISTOON_H
