#include "redistoon.h"
#include <ctype.h>

// Helper function to check if string needs quoting
static bool needs_quoting(const char *str) {
    if (!str || !*str) return true;  // Empty string needs quotes

    // Check for keywords
    if (strcmp(str, "null") == 0 || strcmp(str, "true") == 0 ||
        strcmp(str, "false") == 0) {
        return true;
    }

    // Check if it looks like a number
    char *endptr;
    strtod(str, &endptr);
    if (*endptr == '\0') return true;  // It's a valid number

    // Check for leading/trailing whitespace or special characters
    size_t len = strlen(str);
    if (isspace(str[0]) || isspace(str[len - 1])) return true;

    // Check for structural characters
    for (const char *p = str; *p; p++) {
        if (*p == ',' || *p == ':' || *p == '\n' || *p == '\r' ||
            *p == '{' || *p == '}' || *p == '[' || *p == ']' ||
            *p < 32) {  // Control characters
            return true;
        }
    }

    return false;
}

// Helper function to escape string for TOON
static char *escape_string(const char *str) {
    size_t len = strlen(str);
    size_t escaped_len = len * 2 + 3;  // Worst case: all chars escaped + quotes + null
    char *escaped = malloc(escaped_len);
    if (!escaped) return NULL;

    char *p = escaped;
    *p++ = '"';

    for (const char *s = str; *s; s++) {
        switch (*s) {
            case '"':  *p++ = '\\'; *p++ = '"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\n': *p++ = '\\'; *p++ = 'n'; break;
            case '\r': *p++ = '\\'; *p++ = 'r'; break;
            case '\t': *p++ = '\\'; *p++ = 't'; break;
            default:   *p++ = *s; break;
        }
    }

    *p++ = '"';
    *p = '\0';

    return escaped;
}

// Helper function to create indentation
static char *make_indent(int level) {
    int spaces = level * 2;  // 2 spaces per indent level
    char *indent = malloc(spaces + 1);
    if (!indent) return NULL;
    memset(indent, ' ', spaces);
    indent[spaces] = '\0';
    return indent;
}

// Forward declaration for recursive encoding
static char *encode_value(ToonValue *value, int indent_level, bool inline_mode);

// Encode a tabular array
static char *encode_tabular_array(ToonTabularArray *tab, int indent_level) {
    char *result = malloc(65536);  // Start with 64KB buffer
    if (!result) return NULL;

    char *indent = make_indent(indent_level);
    if (!indent) {
        free(result);
        return NULL;
    }

    // Format: [num_rows,]{header1,header2,...}:
    char *p = result;
    p += sprintf(p, "[%zu,]{", tab->num_rows);

    for (size_t i = 0; i < tab->num_headers; i++) {
        if (i > 0) *p++ = ',';
        p += sprintf(p, "%s", tab->headers[i]);
    }
    p += sprintf(p, "}:\n");

    // Encode each row
    for (size_t row = 0; row < tab->num_rows; row++) {
        p += sprintf(p, "%s", indent);
        for (size_t col = 0; col < tab->num_headers; col++) {
            if (col > 0) *p++ = ',';
            char *cell = encode_value(tab->rows[row][col], 0, true);
            if (cell) {
                p += sprintf(p, "%s", cell);
                free(cell);
            }
        }
        *p++ = '\n';
    }

    free(indent);
    return result;
}

// Encode a simple array
static char *encode_array(ToonValue *value, int indent_level, bool inline_mode) {
    size_t len = value->value.array.length;

    // Check if all elements are primitives (for compact format)
    bool all_primitives = true;
    for (size_t i = 0; i < len; i++) {
        ToonType type = value->value.array.elements[i]->type;
        if (type == TOON_OBJECT || type == TOON_ARRAY || type == TOON_TABULAR_ARRAY) {
            all_primitives = false;
            break;
        }
    }

    char *result = malloc(65536);
    if (!result) return NULL;
    char *p = result;

    if (all_primitives) {
        // Compact format: [N]: val1,val2,val3
        p += sprintf(p, "[%zu]: ", len);
        for (size_t i = 0; i < len; i++) {
            if (i > 0) *p++ = ',';
            char *elem = encode_value(value->value.array.elements[i], 0, true);
            if (elem) {
                p += sprintf(p, "%s", elem);
                free(elem);
            }
        }
    } else {
        // Multi-line format for complex arrays
        p += sprintf(p, "[%zu]:\n", len);
        char *indent = make_indent(indent_level + 1);
        for (size_t i = 0; i < len; i++) {
            p += sprintf(p, "%s- ", indent);
            char *elem = encode_value(value->value.array.elements[i], indent_level + 1, false);
            if (elem) {
                p += sprintf(p, "%s\n", elem);
                free(elem);
            }
        }
        free(indent);
    }

    return result;
}

// Encode an object
static char *encode_object(ToonValue *value, int indent_level, bool inline_mode) {
    char *result = malloc(65536);
    if (!result) return NULL;
    char *p = result;

    char *indent = make_indent(indent_level);
    if (!indent) {
        free(result);
        return NULL;
    }

    for (size_t i = 0; i < value->value.object.length; i++) {
        ToonObjectEntry *entry = &value->value.object.entries[i];

        if (!inline_mode && i > 0) {
            p += sprintf(p, "%s", indent);
        } else if (inline_mode && i > 0) {
            *p++ = ',';
            *p++ = ' ';
        }

        p += sprintf(p, "%s: ", entry->key);

        char *val = encode_value(entry->value, indent_level + 1, false);
        if (val) {
            p += sprintf(p, "%s", val);
            free(val);
        }

        if (!inline_mode) *p++ = '\n';
    }

    free(indent);
    return result;
}

// Main value encoder
static char *encode_value(ToonValue *value, int indent_level, bool inline_mode) {
    if (!value) return strdup("null");

    char *result = malloc(4096);
    if (!result) return NULL;

    switch (value->type) {
        case TOON_NULL:
            strcpy(result, "null");
            break;

        case TOON_BOOLEAN:
            strcpy(result, value->value.boolean ? "true" : "false");
            break;

        case TOON_NUMBER: {
            // Format number, removing unnecessary trailing zeros
            double num = value->value.number;
            if (num == (long)num) {
                sprintf(result, "%ld", (long)num);
            } else {
                sprintf(result, "%.10g", num);
            }
            break;
        }

        case TOON_STRING: {
            if (needs_quoting(value->value.string)) {
                char *escaped = escape_string(value->value.string);
                if (escaped) {
                    strcpy(result, escaped);
                    free(escaped);
                }
            } else {
                strcpy(result, value->value.string);
            }
            break;
        }

        case TOON_ARRAY: {
            free(result);
            result = encode_array(value, indent_level, inline_mode);
            break;
        }

        case TOON_OBJECT: {
            free(result);
            result = encode_object(value, indent_level, inline_mode);
            break;
        }

        case TOON_TABULAR_ARRAY: {
            free(result);
            result = encode_tabular_array(&value->value.tabular, indent_level);
            break;
        }

        default:
            strcpy(result, "null");
            break;
    }

    return result;
}

// Public encoding function
char *toon_encode(ToonValue *value, int indent_level) {
    return encode_value(value, indent_level, false);
}
