#include "redistoon.h"
#include <ctype.h>

// Simple path parser - supports basic JSONPath-like syntax
// Examples:
//   $ - root
//   $.name - object property
//   $.users[0] - array index
//   $.users[*] - all array elements
//   $.users[*].name - all names in users array

typedef struct {
    char **segments;
    size_t num_segments;
} Path;

// Parse a path string into segments
static Path *path_parse(const char *path_str) {
    if (!path_str || *path_str != '$') {
        return NULL;
    }

    Path *path = malloc(sizeof(Path));
    if (!path) return NULL;

    path->segments = malloc(sizeof(char *) * 64);
    path->num_segments = 0;

    const char *p = path_str + 1;  // Skip $

    while (*p) {
        if (*p == '.') {
            p++;  // Skip .

            // Parse property name
            char segment[256];
            size_t i = 0;
            while (*p && *p != '.' && *p != '[' && i < 255) {
                segment[i++] = *p++;
            }
            segment[i] = '\0';

            if (i > 0) {
                path->segments[path->num_segments++] = strdup(segment);
            }
        } else if (*p == '[') {
            p++;  // Skip [

            // Parse array index or wildcard
            char segment[256];
            size_t i = 0;
            while (*p && *p != ']' && i < 255) {
                segment[i++] = *p++;
            }
            segment[i] = '\0';

            if (*p == ']') p++;  // Skip ]

            if (i > 0) {
                char index_segment[260];
                sprintf(index_segment, "[%s]", segment);
                path->segments[path->num_segments++] = strdup(index_segment);
            }
        } else {
            break;
        }
    }

    return path;
}

// Free a parsed path
static void path_free(Path *path) {
    if (!path) return;

    for (size_t i = 0; i < path->num_segments; i++) {
        free(path->segments[i]);
    }
    free(path->segments);
    free(path);
}

// Navigate to a value using path
static ToonValue *path_navigate(ToonValue *root, Path *path, size_t segment_index) {
    if (!root || !path || segment_index >= path->num_segments) {
        return root;
    }

    const char *segment = path->segments[segment_index];

    // Handle array indexing
    if (segment[0] == '[') {
        if (root->type != TOON_ARRAY && root->type != TOON_TABULAR_ARRAY) {
            return NULL;
        }

        // Check for wildcard
        if (strcmp(segment, "[*]") == 0) {
            // For now, return NULL for wildcards (requires multiple results)
            return NULL;
        }

        // Parse index
        int index = atoi(segment + 1);

        if (root->type == TOON_ARRAY) {
            if (index < 0) {
                index = root->value.array.length + index;
            }

            if (index < 0 || (size_t)index >= root->value.array.length) {
                return NULL;
            }

            return path_navigate(root->value.array.elements[index], path, segment_index + 1);
        } else if (root->type == TOON_TABULAR_ARRAY) {
            if (index < 0) {
                index = root->value.tabular.num_rows + index;
            }

            if (index < 0 || (size_t)index >= root->value.tabular.num_rows) {
                return NULL;
            }

            // For tabular arrays, we need to return a row as an object
            // For simplicity, just return NULL for now
            return NULL;
        }
    }

    // Handle object property access
    if (root->type == TOON_OBJECT) {
        for (size_t i = 0; i < root->value.object.length; i++) {
            if (strcmp(root->value.object.entries[i].key, segment) == 0) {
                return path_navigate(root->value.object.entries[i].value, path, segment_index + 1);
            }
        }
        return NULL;
    }

    return NULL;
}

// Public path get function
ToonValue *toon_path_get(ToonValue *root, const char *path_str) {
    if (!root || !path_str) return NULL;

    // Handle root path
    if (strcmp(path_str, "$") == 0) {
        return root;
    }

    Path *path = path_parse(path_str);
    if (!path) return NULL;

    ToonValue *result = path_navigate(root, path, 0);

    path_free(path);
    return result;
}

// Set value at path (simplified version - doesn't handle all cases)
int toon_path_set(ToonValue *root, const char *path_str, ToonValue *value) {
    if (!root || !path_str || !value) return -1;

    // Handle root path
    if (strcmp(path_str, "$") == 0) {
        // Can't replace root directly, need to copy value contents
        return -1;
    }

    Path *path = path_parse(path_str);
    if (!path || path->num_segments == 0) {
        path_free(path);
        return -1;
    }

    // Navigate to parent
    ToonValue *parent = root;
    for (size_t i = 0; i < path->num_segments - 1; i++) {
        parent = path_navigate(parent, path, i);
        if (!parent) {
            path_free(path);
            return -1;
        }
    }

    const char *last_segment = path->segments[path->num_segments - 1];

    // Handle array indexing
    if (last_segment[0] == '[') {
        if (parent->type != TOON_ARRAY) {
            path_free(path);
            return -1;
        }

        int index = atoi(last_segment + 1);
        if (index < 0) {
            index = parent->value.array.length + index;
        }

        if (index < 0 || (size_t)index >= parent->value.array.length) {
            path_free(path);
            return -1;
        }

        toon_value_free(parent->value.array.elements[index]);
        parent->value.array.elements[index] = value;

        path_free(path);
        return 0;
    }

    // Handle object property
    if (parent->type == TOON_OBJECT) {
        for (size_t i = 0; i < parent->value.object.length; i++) {
            if (strcmp(parent->value.object.entries[i].key, last_segment) == 0) {
                toon_value_free(parent->value.object.entries[i].value);
                parent->value.object.entries[i].value = value;
                path_free(path);
                return 0;
            }
        }

        // Property doesn't exist, add it
        size_t new_length = parent->value.object.length + 1;
        ToonObjectEntry *new_entries = realloc(parent->value.object.entries,
                                               sizeof(ToonObjectEntry) * new_length);
        if (!new_entries) {
            path_free(path);
            return -1;
        }

        new_entries[parent->value.object.length].key = strdup(last_segment);
        new_entries[parent->value.object.length].value = value;
        parent->value.object.entries = new_entries;
        parent->value.object.length = new_length;

        path_free(path);
        return 0;
    }

    path_free(path);
    return -1;
}

// Delete value at path
int toon_path_delete(ToonValue *root, const char *path_str) {
    if (!root || !path_str) return -1;

    // Can't delete root
    if (strcmp(path_str, "$") == 0) {
        return -1;
    }

    Path *path = path_parse(path_str);
    if (!path || path->num_segments == 0) {
        path_free(path);
        return -1;
    }

    // Navigate to parent
    ToonValue *parent = root;
    for (size_t i = 0; i < path->num_segments - 1; i++) {
        parent = path_navigate(parent, path, i);
        if (!parent) {
            path_free(path);
            return -1;
        }
    }

    const char *last_segment = path->segments[path->num_segments - 1];

    // Handle array indexing
    if (last_segment[0] == '[') {
        if (parent->type != TOON_ARRAY) {
            path_free(path);
            return -1;
        }

        int index = atoi(last_segment + 1);
        if (index < 0) {
            index = parent->value.array.length + index;
        }

        if (index < 0 || (size_t)index >= parent->value.array.length) {
            path_free(path);
            return -1;
        }

        // Free the element and shift remaining elements
        toon_value_free(parent->value.array.elements[index]);

        for (size_t i = index; i < parent->value.array.length - 1; i++) {
            parent->value.array.elements[i] = parent->value.array.elements[i + 1];
        }

        parent->value.array.length--;

        path_free(path);
        return 0;
    }

    // Handle object property
    if (parent->type == TOON_OBJECT) {
        for (size_t i = 0; i < parent->value.object.length; i++) {
            if (strcmp(parent->value.object.entries[i].key, last_segment) == 0) {
                free(parent->value.object.entries[i].key);
                toon_value_free(parent->value.object.entries[i].value);

                // Shift remaining entries
                for (size_t j = i; j < parent->value.object.length - 1; j++) {
                    parent->value.object.entries[j] = parent->value.object.entries[j + 1];
                }

                parent->value.object.length--;

                path_free(path);
                return 0;
            }
        }
    }

    path_free(path);
    return -1;
}
