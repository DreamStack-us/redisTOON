#include "redistoon.h"

// Create a new TOON value
ToonValue *toon_value_create(ToonType type) {
    ToonValue *value = calloc(1, sizeof(ToonValue));
    if (!value) return NULL;

    value->type = type;

    switch (type) {
        case TOON_NULL:
        case TOON_BOOLEAN:
        case TOON_NUMBER:
            // No additional initialization needed
            break;

        case TOON_STRING:
            value->value.string = NULL;
            break;

        case TOON_ARRAY:
            value->value.array.elements = NULL;
            value->value.array.length = 0;
            break;

        case TOON_OBJECT:
            value->value.object.entries = NULL;
            value->value.object.length = 0;
            break;

        case TOON_TABULAR_ARRAY:
            value->value.tabular.headers = NULL;
            value->value.tabular.num_headers = 0;
            value->value.tabular.rows = NULL;
            value->value.tabular.num_rows = 0;
            break;
    }

    return value;
}

// Free a TOON value
void toon_value_free(ToonValue *value) {
    if (!value) return;

    switch (value->type) {
        case TOON_STRING:
            if (value->value.string) {
                free(value->value.string);
            }
            break;

        case TOON_ARRAY:
            if (value->value.array.elements) {
                for (size_t i = 0; i < value->value.array.length; i++) {
                    toon_value_free(value->value.array.elements[i]);
                }
                free(value->value.array.elements);
            }
            break;

        case TOON_OBJECT:
            if (value->value.object.entries) {
                for (size_t i = 0; i < value->value.object.length; i++) {
                    if (value->value.object.entries[i].key) {
                        free(value->value.object.entries[i].key);
                    }
                    toon_value_free(value->value.object.entries[i].value);
                }
                free(value->value.object.entries);
            }
            break;

        case TOON_TABULAR_ARRAY:
            if (value->value.tabular.headers) {
                for (size_t i = 0; i < value->value.tabular.num_headers; i++) {
                    if (value->value.tabular.headers[i]) {
                        free(value->value.tabular.headers[i]);
                    }
                }
                free(value->value.tabular.headers);
            }
            if (value->value.tabular.rows) {
                for (size_t i = 0; i < value->value.tabular.num_rows; i++) {
                    if (value->value.tabular.rows[i]) {
                        for (size_t j = 0; j < value->value.tabular.num_headers; j++) {
                            toon_value_free(value->value.tabular.rows[i][j]);
                        }
                        free(value->value.tabular.rows[i]);
                    }
                }
                free(value->value.tabular.rows);
            }
            break;

        case TOON_NULL:
        case TOON_BOOLEAN:
        case TOON_NUMBER:
            // No additional cleanup needed
            break;
    }

    free(value);
}

// Create a new TOON document
ToonDocument *toon_document_create(void) {
    ToonDocument *doc = calloc(1, sizeof(ToonDocument));
    if (!doc) return NULL;

    doc->root = toon_value_create(TOON_NULL);
    return doc;
}

// Free a TOON document
void toon_document_free(ToonDocument *doc) {
    if (!doc) return;

    if (doc->root) {
        toon_value_free(doc->root);
    }

    free(doc);
}

// Get type string
const char *toon_type_string(ToonType type) {
    switch (type) {
        case TOON_NULL:          return "null";
        case TOON_BOOLEAN:       return "boolean";
        case TOON_NUMBER:        return "number";
        case TOON_STRING:        return "string";
        case TOON_ARRAY:         return "array";
        case TOON_OBJECT:        return "object";
        case TOON_TABULAR_ARRAY: return "tabular_array";
        default:                 return "unknown";
    }
}

// Estimate token count for a TOON value (approximate)
size_t toon_estimate_tokens(ToonValue *value) {
    if (!value) return 0;

    size_t tokens = 0;

    switch (value->type) {
        case TOON_NULL:
            tokens = 1;
            break;

        case TOON_BOOLEAN:
            tokens = 1;
            break;

        case TOON_NUMBER:
            tokens = 1;
            break;

        case TOON_STRING:
            // Rough estimate: 1 token per 4 characters
            tokens = (strlen(value->value.string) / 4) + 1;
            break;

        case TOON_ARRAY:
            tokens = 2;  // [N]:
            for (size_t i = 0; i < value->value.array.length; i++) {
                tokens += toon_estimate_tokens(value->value.array.elements[i]);
            }
            break;

        case TOON_OBJECT:
            for (size_t i = 0; i < value->value.object.length; i++) {
                tokens += (strlen(value->value.object.entries[i].key) / 4) + 2;  // key:
                tokens += toon_estimate_tokens(value->value.object.entries[i].value);
            }
            break;

        case TOON_TABULAR_ARRAY: {
            // Headers
            tokens += 3;  // [N,]{}:
            for (size_t i = 0; i < value->value.tabular.num_headers; i++) {
                tokens += (strlen(value->value.tabular.headers[i]) / 4) + 1;
            }
            // Rows
            for (size_t i = 0; i < value->value.tabular.num_rows; i++) {
                for (size_t j = 0; j < value->value.tabular.num_headers; j++) {
                    tokens += toon_estimate_tokens(value->value.tabular.rows[i][j]);
                }
            }
            break;
        }
    }

    return tokens;
}
