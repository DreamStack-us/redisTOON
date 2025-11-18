#include "redistoon.h"

// ============================================================================
// Array Operations
// ============================================================================

// Append value(s) to array at path
int toon_array_append(ToonValue *root, const char *path_str, ToonValue **values, size_t num_values) {
    if (!root || !path_str || !values || num_values == 0) return -1;

    ToonValue *target = toon_path_get(root, path_str);
    if (!target) return -1;

    // Only works on arrays
    if (target->type != TOON_ARRAY) {
        return -1;
    }

    // Expand array
    size_t new_length = target->value.array.length + num_values;
    ToonValue **new_elements = realloc(target->value.array.elements,
                                       sizeof(ToonValue *) * new_length);
    if (!new_elements) return -1;

    // Append new values
    for (size_t i = 0; i < num_values; i++) {
        new_elements[target->value.array.length + i] = values[i];
    }

    target->value.array.elements = new_elements;
    target->value.array.length = new_length;

    return new_length;
}

// Insert value into array at specific index
int toon_array_insert(ToonValue *root, const char *path_str, long index, ToonValue *value) {
    if (!root || !path_str || !value) return -1;

    ToonValue *target = toon_path_get(root, path_str);
    if (!target || target->type != TOON_ARRAY) {
        return -1;
    }

    size_t len = target->value.array.length;

    // Handle negative indices
    if (index < 0) {
        index = len + index;
    }

    // Bounds check
    if (index < 0 || (size_t)index > len) {
        return -1;
    }

    // Expand array
    ToonValue **new_elements = malloc(sizeof(ToonValue *) * (len + 1));
    if (!new_elements) return -1;

    // Copy elements before insertion point
    for (size_t i = 0; i < (size_t)index; i++) {
        new_elements[i] = target->value.array.elements[i];
    }

    // Insert new value
    new_elements[index] = value;

    // Copy elements after insertion point
    for (size_t i = index; i < len; i++) {
        new_elements[i + 1] = target->value.array.elements[i];
    }

    free(target->value.array.elements);
    target->value.array.elements = new_elements;
    target->value.array.length = len + 1;

    return len + 1;
}

// Pop value from array (remove and return)
ToonValue *toon_array_pop(ToonValue *root, const char *path_str, long index) {
    if (!root || !path_str) return NULL;

    ToonValue *target = toon_path_get(root, path_str);
    if (!target || target->type != TOON_ARRAY) {
        return NULL;
    }

    size_t len = target->value.array.length;
    if (len == 0) return NULL;

    // Handle negative indices
    if (index < 0) {
        index = len + index;
    }

    // Bounds check
    if (index < 0 || (size_t)index >= len) {
        return NULL;
    }

    // Get the value to pop
    ToonValue *popped = target->value.array.elements[index];

    // Shift remaining elements
    for (size_t i = index; i < len - 1; i++) {
        target->value.array.elements[i] = target->value.array.elements[i + 1];
    }

    target->value.array.length--;

    return popped;
}

// Get array length
long toon_array_length(ToonValue *root, const char *path_str) {
    if (!root || !path_str) return -1;

    ToonValue *target = toon_path_get(root, path_str);
    if (!target) return -1;

    if (target->type == TOON_ARRAY) {
        return target->value.array.length;
    } else if (target->type == TOON_TABULAR_ARRAY) {
        return target->value.tabular.num_rows;
    }

    return -1;
}

// ============================================================================
// Merge Operations
// ============================================================================

// Deep copy a TOON value
static ToonValue *toon_value_copy(ToonValue *src) {
    if (!src) return NULL;

    ToonValue *dst = toon_value_create(src->type);
    if (!dst) return NULL;

    switch (src->type) {
        case TOON_NULL:
            break;

        case TOON_BOOLEAN:
            dst->value.boolean = src->value.boolean;
            break;

        case TOON_NUMBER:
            dst->value.number = src->value.number;
            break;

        case TOON_STRING:
            dst->value.string = strdup(src->value.string);
            break;

        case TOON_ARRAY:
            dst->value.array.length = src->value.array.length;
            dst->value.array.elements = malloc(sizeof(ToonValue *) * src->value.array.length);
            for (size_t i = 0; i < src->value.array.length; i++) {
                dst->value.array.elements[i] = toon_value_copy(src->value.array.elements[i]);
            }
            break;

        case TOON_OBJECT:
            dst->value.object.length = src->value.object.length;
            dst->value.object.entries = malloc(sizeof(ToonObjectEntry) * src->value.object.length);
            for (size_t i = 0; i < src->value.object.length; i++) {
                dst->value.object.entries[i].key = strdup(src->value.object.entries[i].key);
                dst->value.object.entries[i].value = toon_value_copy(src->value.object.entries[i].value);
            }
            break;

        case TOON_TABULAR_ARRAY:
            dst->value.tabular.num_headers = src->value.tabular.num_headers;
            dst->value.tabular.num_rows = src->value.tabular.num_rows;

            dst->value.tabular.headers = malloc(sizeof(char *) * src->value.tabular.num_headers);
            for (size_t i = 0; i < src->value.tabular.num_headers; i++) {
                dst->value.tabular.headers[i] = strdup(src->value.tabular.headers[i]);
            }

            dst->value.tabular.rows = malloc(sizeof(ToonValue **) * src->value.tabular.num_rows);
            for (size_t i = 0; i < src->value.tabular.num_rows; i++) {
                dst->value.tabular.rows[i] = malloc(sizeof(ToonValue *) * src->value.tabular.num_headers);
                for (size_t j = 0; j < src->value.tabular.num_headers; j++) {
                    dst->value.tabular.rows[i][j] = toon_value_copy(src->value.tabular.rows[i][j]);
                }
            }
            break;
    }

    return dst;
}

// Merge two TOON objects (target is modified)
int toon_merge(ToonValue *target, ToonValue *source) {
    if (!target || !source) return -1;

    // Can only merge objects
    if (target->type != TOON_OBJECT || source->type != TOON_OBJECT) {
        return -1;
    }

    // Process each entry in source
    for (size_t i = 0; i < source->value.object.length; i++) {
        char *key = source->value.object.entries[i].key;
        ToonValue *src_value = source->value.object.entries[i].value;

        // Find if key exists in target
        bool found = false;
        for (size_t j = 0; j < target->value.object.length; j++) {
            if (strcmp(target->value.object.entries[j].key, key) == 0) {
                found = true;

                // If both are objects, recursively merge
                if (target->value.object.entries[j].value->type == TOON_OBJECT &&
                    src_value->type == TOON_OBJECT) {
                    toon_merge(target->value.object.entries[j].value, src_value);
                } else {
                    // Otherwise, replace with copy of source value
                    toon_value_free(target->value.object.entries[j].value);
                    target->value.object.entries[j].value = toon_value_copy(src_value);
                }
                break;
            }
        }

        // If key doesn't exist, add it
        if (!found) {
            size_t new_length = target->value.object.length + 1;
            ToonObjectEntry *new_entries = realloc(target->value.object.entries,
                                                   sizeof(ToonObjectEntry) * new_length);
            if (!new_entries) return -1;

            new_entries[target->value.object.length].key = strdup(key);
            new_entries[target->value.object.length].value = toon_value_copy(src_value);

            target->value.object.entries = new_entries;
            target->value.object.length = new_length;
        }
    }

    return 0;
}

// ============================================================================
// Validation
// ============================================================================

// Validate TOON value structure
bool toon_validate(ToonValue *value, char **error) {
    if (!value) {
        if (error) *error = strdup("NULL value");
        return false;
    }

    switch (value->type) {
        case TOON_NULL:
        case TOON_BOOLEAN:
        case TOON_NUMBER:
            return true;

        case TOON_STRING:
            if (!value->value.string) {
                if (error) *error = strdup("String value is NULL");
                return false;
            }
            return true;

        case TOON_ARRAY:
            if (value->value.array.length > 0 && !value->value.array.elements) {
                if (error) *error = strdup("Array has length but NULL elements");
                return false;
            }
            for (size_t i = 0; i < value->value.array.length; i++) {
                if (!toon_validate(value->value.array.elements[i], error)) {
                    return false;
                }
            }
            return true;

        case TOON_OBJECT:
            if (value->value.object.length > 0 && !value->value.object.entries) {
                if (error) *error = strdup("Object has length but NULL entries");
                return false;
            }
            for (size_t i = 0; i < value->value.object.length; i++) {
                if (!value->value.object.entries[i].key) {
                    if (error) *error = strdup("Object entry has NULL key");
                    return false;
                }
                if (!toon_validate(value->value.object.entries[i].value, error)) {
                    return false;
                }
            }
            return true;

        case TOON_TABULAR_ARRAY:
            if (value->value.tabular.num_headers == 0) {
                if (error) *error = strdup("Tabular array has no headers");
                return false;
            }
            if (!value->value.tabular.headers) {
                if (error) *error = strdup("Tabular array has NULL headers");
                return false;
            }
            for (size_t i = 0; i < value->value.tabular.num_headers; i++) {
                if (!value->value.tabular.headers[i]) {
                    if (error) *error = strdup("Tabular array has NULL header");
                    return false;
                }
            }
            if (value->value.tabular.num_rows > 0) {
                if (!value->value.tabular.rows) {
                    if (error) *error = strdup("Tabular array has rows but NULL rows pointer");
                    return false;
                }
                for (size_t i = 0; i < value->value.tabular.num_rows; i++) {
                    if (!value->value.tabular.rows[i]) {
                        if (error) *error = strdup("Tabular array has NULL row");
                        return false;
                    }
                    for (size_t j = 0; j < value->value.tabular.num_headers; j++) {
                        if (!toon_validate(value->value.tabular.rows[i][j], error)) {
                            return false;
                        }
                    }
                }
            }
            return true;

        default:
            if (error) *error = strdup("Unknown type");
            return false;
    }
}
