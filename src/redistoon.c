#include "redistoon.h"

// Redis module type
RedisModuleType *ToonType_RMT = NULL;

// ============================================================================
// Redis Type Methods
// ============================================================================

void *ToonTypeRdbLoad(RedisModuleIO *rdb, int encver) {
    if (encver != 0) return NULL;

    size_t len;
    char *toon_str = RedisModule_LoadStringBuffer(rdb, &len);
    if (!toon_str) return NULL;

    char *error = NULL;
    ToonValue *value = toon_decode(toon_str, &error);
    RedisModule_Free(toon_str);

    if (!value) {
        if (error) free(error);
        return NULL;
    }

    ToonDocument *doc = toon_document_create();
    if (doc) {
        toon_value_free(doc->root);
        doc->root = value;
    }

    return doc;
}

void ToonTypeRdbSave(RedisModuleIO *rdb, void *value) {
    ToonDocument *doc = value;
    if (!doc || !doc->root) return;

    char *toon_str = toon_encode(doc->root, 0);
    if (toon_str) {
        RedisModule_SaveStringBuffer(rdb, toon_str, strlen(toon_str));
        free(toon_str);
    }
}

void ToonTypeAofRewrite(RedisModuleIO *aof, RedisModuleString *key, void *value) {
    ToonDocument *doc = value;
    if (!doc || !doc->root) return;

    char *toon_str = toon_encode(doc->root, 0);
    if (toon_str) {
        RedisModule_EmitAOF(aof, "TOON.SET", "ssc", key, "$", toon_str, strlen(toon_str));
        free(toon_str);
    }
}

void ToonTypeDigest(RedisModuleDigest *md, void *value) {
    // Not implemented yet
}

void ToonTypeFree(void *value) {
    toon_document_free((ToonDocument *)value);
}

// ============================================================================
// Command: TOON.SET key path value
// ============================================================================

int ToonSet_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 4) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModule_AutoMemory(ctx);

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);

    size_t path_len;
    const char *path = RedisModule_StringPtrLen(argv[2], &path_len);

    size_t value_len;
    const char *value_str = RedisModule_StringPtrLen(argv[3], &value_len);

    // Parse the TOON value
    char *error = NULL;
    ToonValue *value = toon_decode(value_str, &error);
    if (!value) {
        RedisModule_ReplyWithError(ctx, error ? error : "ERR invalid TOON format");
        if (error) free(error);
        return REDISMODULE_OK;
    }

    // Get or create document
    ToonDocument *doc;
    int type = RedisModule_KeyType(key);

    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        doc = toon_document_create();
        RedisModule_ModuleTypeSetValue(key, ToonType_RMT, doc);
    } else if (type == REDISMODULE_KEYTYPE_MODULE &&
               RedisModule_ModuleTypeGetType(key) == ToonType_RMT) {
        doc = RedisModule_ModuleTypeGetValue(key);
    } else {
        toon_value_free(value);
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    // Set value at path
    if (strcmp(path, "$") == 0) {
        toon_value_free(doc->root);
        doc->root = value;
    } else {
        if (toon_path_set(doc->root, path, value) != 0) {
            toon_value_free(value);
            return RedisModule_ReplyWithError(ctx, "ERR invalid path");
        }
    }

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    RedisModule_ReplicateVerbatim(ctx);

    return REDISMODULE_OK;
}

// ============================================================================
// Command: TOON.GET key [path]
// ============================================================================

int ToonGet_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 2 || argc > 3) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModule_AutoMemory(ctx);

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);

    int type = RedisModule_KeyType(key);
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        return RedisModule_ReplyWithNull(ctx);
    }

    if (type != REDISMODULE_KEYTYPE_MODULE ||
        RedisModule_ModuleTypeGetType(key) != ToonType_RMT) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    ToonDocument *doc = RedisModule_ModuleTypeGetValue(key);
    if (!doc || !doc->root) {
        return RedisModule_ReplyWithNull(ctx);
    }

    // Get path (default to root)
    const char *path = "$";
    if (argc == 3) {
        size_t path_len;
        path = RedisModule_StringPtrLen(argv[2], &path_len);
    }

    ToonValue *value;
    if (strcmp(path, "$") == 0) {
        value = doc->root;
    } else {
        value = toon_path_get(doc->root, path);
    }

    if (!value) {
        return RedisModule_ReplyWithNull(ctx);
    }

    // Encode and return
    char *result = toon_encode(value, 0);
    if (result) {
        RedisModule_ReplyWithStringBuffer(ctx, result, strlen(result));
        free(result);
    } else {
        return RedisModule_ReplyWithNull(ctx);
    }

    return REDISMODULE_OK;
}

// ============================================================================
// Command: TOON.DEL key path
// ============================================================================

int ToonDel_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModule_AutoMemory(ctx);

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);

    int type = RedisModule_KeyType(key);
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        return RedisModule_ReplyWithLongLong(ctx, 0);
    }

    if (type != REDISMODULE_KEYTYPE_MODULE ||
        RedisModule_ModuleTypeGetType(key) != ToonType_RMT) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    ToonDocument *doc = RedisModule_ModuleTypeGetValue(key);
    if (!doc || !doc->root) {
        return RedisModule_ReplyWithLongLong(ctx, 0);
    }

    size_t path_len;
    const char *path = RedisModule_StringPtrLen(argv[2], &path_len);

    if (toon_path_delete(doc->root, path) == 0) {
        RedisModule_ReplyWithLongLong(ctx, 1);
        RedisModule_ReplicateVerbatim(ctx);
    } else {
        RedisModule_ReplyWithLongLong(ctx, 0);
    }

    return REDISMODULE_OK;
}

// ============================================================================
// Command: TOON.TYPE key path
// ============================================================================

int ToonType_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModule_AutoMemory(ctx);

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);

    int type = RedisModule_KeyType(key);
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        return RedisModule_ReplyWithNull(ctx);
    }

    if (type != REDISMODULE_KEYTYPE_MODULE ||
        RedisModule_ModuleTypeGetType(key) != ToonType_RMT) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    ToonDocument *doc = RedisModule_ModuleTypeGetValue(key);
    if (!doc || !doc->root) {
        return RedisModule_ReplyWithNull(ctx);
    }

    size_t path_len;
    const char *path = RedisModule_StringPtrLen(argv[2], &path_len);

    ToonValue *value = toon_path_get(doc->root, path);
    if (!value) {
        return RedisModule_ReplyWithNull(ctx);
    }

    RedisModule_ReplyWithSimpleString(ctx, toon_type_string(value->type));
    return REDISMODULE_OK;
}

// ============================================================================
// Command: TOON.TOJSON key [path]
// ============================================================================

int ToonToJson_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 2 || argc > 3) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModule_AutoMemory(ctx);

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);

    int type = RedisModule_KeyType(key);
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        return RedisModule_ReplyWithNull(ctx);
    }

    if (type != REDISMODULE_KEYTYPE_MODULE ||
        RedisModule_ModuleTypeGetType(key) != ToonType_RMT) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    ToonDocument *doc = RedisModule_ModuleTypeGetValue(key);
    if (!doc || !doc->root) {
        return RedisModule_ReplyWithNull(ctx);
    }

    const char *path = "$";
    if (argc == 3) {
        size_t path_len;
        path = RedisModule_StringPtrLen(argv[2], &path_len);
    }

    ToonValue *value = toon_path_get(doc->root, path);
    if (!value) {
        return RedisModule_ReplyWithNull(ctx);
    }

    char *json = toon_to_json(value);
    if (json) {
        RedisModule_ReplyWithStringBuffer(ctx, json, strlen(json));
        free(json);
    } else {
        return RedisModule_ReplyWithNull(ctx);
    }

    return REDISMODULE_OK;
}

// ============================================================================
// Command: TOON.FROMJSON key json
// ============================================================================

int ToonFromJson_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModule_AutoMemory(ctx);

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);

    size_t json_len;
    const char *json_str = RedisModule_StringPtrLen(argv[2], &json_len);

    char *error = NULL;
    ToonValue *value = json_to_toon(json_str, &error);
    if (!value) {
        RedisModule_ReplyWithError(ctx, error ? error : "ERR invalid JSON");
        if (error) free(error);
        return REDISMODULE_OK;
    }

    ToonDocument *doc;
    int type = RedisModule_KeyType(key);

    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        doc = toon_document_create();
        RedisModule_ModuleTypeSetValue(key, ToonType_RMT, doc);
    } else if (type == REDISMODULE_KEYTYPE_MODULE &&
               RedisModule_ModuleTypeGetType(key) == ToonType_RMT) {
        doc = RedisModule_ModuleTypeGetValue(key);
    } else {
        toon_value_free(value);
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    toon_value_free(doc->root);
    doc->root = value;

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    RedisModule_ReplicateVerbatim(ctx);

    return REDISMODULE_OK;
}

// ============================================================================
// Command: TOON.TOKENCOUNT key [path]
// ============================================================================

int ToonTokenCount_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 2 || argc > 3) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModule_AutoMemory(ctx);

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);

    int type = RedisModule_KeyType(key);
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        return RedisModule_ReplyWithLongLong(ctx, 0);
    }

    if (type != REDISMODULE_KEYTYPE_MODULE ||
        RedisModule_ModuleTypeGetType(key) != ToonType_RMT) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    ToonDocument *doc = RedisModule_ModuleTypeGetValue(key);
    if (!doc || !doc->root) {
        return RedisModule_ReplyWithLongLong(ctx, 0);
    }

    const char *path = "$";
    if (argc == 3) {
        size_t path_len;
        path = RedisModule_StringPtrLen(argv[2], &path_len);
    }

    ToonValue *value = toon_path_get(doc->root, path);
    if (!value) {
        return RedisModule_ReplyWithLongLong(ctx, 0);
    }

    size_t tokens = toon_estimate_tokens(value);
    RedisModule_ReplyWithLongLong(ctx, tokens);

    return REDISMODULE_OK;
}

// ============================================================================
// Module Initialization
// ============================================================================

int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (RedisModule_Init(ctx, REDISTOON_MODULE_NAME, 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    // Register the TOON data type
    RedisModuleTypeMethods tm = {
        .version = REDISMODULE_TYPE_METHOD_VERSION,
        .rdb_load = ToonTypeRdbLoad,
        .rdb_save = ToonTypeRdbSave,
        .aof_rewrite = ToonTypeAofRewrite,
        .free = ToonTypeFree,
        .digest = ToonTypeDigest
    };

    ToonType_RMT = RedisModule_CreateDataType(ctx, "toon-type", 0, &tm);
    if (ToonType_RMT == NULL) {
        return REDISMODULE_ERR;
    }

    // Register commands
    if (RedisModule_CreateCommand(ctx, "toon.set", ToonSet_RedisCommand,
                                   "write deny-oom", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "toon.get", ToonGet_RedisCommand,
                                   "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "toon.del", ToonDel_RedisCommand,
                                   "write", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "toon.type", ToonType_RedisCommand,
                                   "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "toon.tojson", ToonToJson_RedisCommand,
                                   "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "toon.fromjson", ToonFromJson_RedisCommand,
                                   "write deny-oom", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "toon.tokencount", ToonTokenCount_RedisCommand,
                                   "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModule_Log(ctx, "notice", "redisTOON module loaded successfully v%s", REDISTOON_VERSION);

    return REDISMODULE_OK;
}
