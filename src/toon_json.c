#include "redistoon.h"
#include <ctype.h>

// Simple JSON encoder (converts TOON to JSON)
char *toon_to_json(ToonValue *value) {
    if (!value) return strdup("null");

    char *result = malloc(65536);
    if (!result) return NULL;
    char *p = result;

    switch (value->type) {
        case TOON_NULL:
            strcpy(p, "null");
            break;

        case TOON_BOOLEAN:
            strcpy(p, value->value.boolean ? "true" : "false");
            break;

        case TOON_NUMBER:
            if (value->value.number == (long)value->value.number) {
                sprintf(p, "%ld", (long)value->value.number);
            } else {
                sprintf(p, "%.10g", value->value.number);
            }
            break;

        case TOON_STRING: {
            *p++ = '"';
            for (const char *s = value->value.string; *s; s++) {
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
            break;
        }

        case TOON_ARRAY: {
            *p++ = '[';
            for (size_t i = 0; i < value->value.array.length; i++) {
                if (i > 0) *p++ = ',';
                char *elem = toon_to_json(value->value.array.elements[i]);
                if (elem) {
                    strcpy(p, elem);
                    p += strlen(elem);
                    free(elem);
                }
            }
            *p++ = ']';
            *p = '\0';
            break;
        }

        case TOON_OBJECT: {
            *p++ = '{';
            for (size_t i = 0; i < value->value.object.length; i++) {
                if (i > 0) *p++ = ',';

                *p++ = '"';
                strcpy(p, value->value.object.entries[i].key);
                p += strlen(value->value.object.entries[i].key);
                *p++ = '"';
                *p++ = ':';

                char *val = toon_to_json(value->value.object.entries[i].value);
                if (val) {
                    strcpy(p, val);
                    p += strlen(val);
                    free(val);
                }
            }
            *p++ = '}';
            *p = '\0';
            break;
        }

        case TOON_TABULAR_ARRAY: {
            // Convert tabular array to array of objects
            *p++ = '[';
            for (size_t row = 0; row < value->value.tabular.num_rows; row++) {
                if (row > 0) *p++ = ',';
                *p++ = '{';

                for (size_t col = 0; col < value->value.tabular.num_headers; col++) {
                    if (col > 0) *p++ = ',';

                    *p++ = '"';
                    strcpy(p, value->value.tabular.headers[col]);
                    p += strlen(value->value.tabular.headers[col]);
                    *p++ = '"';
                    *p++ = ':';

                    char *cell = toon_to_json(value->value.tabular.rows[row][col]);
                    if (cell) {
                        strcpy(p, cell);
                        p += strlen(cell);
                        free(cell);
                    }
                }

                *p++ = '}';
            }
            *p++ = ']';
            *p = '\0';
            break;
        }
    }

    return result;
}

// Simple JSON parser state
typedef struct {
    const char *json;
    const char *current;
    char *error;
} JsonParser;

// Forward declarations
static ToonValue *parse_json_value(JsonParser *jp);

static void skip_json_whitespace(JsonParser *jp) {
    while (*jp->current && isspace(*jp->current)) {
        jp->current++;
    }
}

static bool json_match(JsonParser *jp, const char *str) {
    size_t len = strlen(str);
    if (strncmp(jp->current, str, len) == 0) {
        jp->current += len;
        return true;
    }
    return false;
}

static char json_peek(JsonParser *jp) {
    return *jp->current;
}

static char json_consume(JsonParser *jp) {
    return *jp->current ? *jp->current++ : '\0';
}

static char *parse_json_string(JsonParser *jp) {
    if (json_consume(jp) != '"') {
        jp->error = strdup("Expected '\"'");
        return NULL;
    }

    char buffer[4096];
    char *buf = buffer;
    size_t remaining = sizeof(buffer) - 1;

    while (json_peek(jp) != '"' && json_peek(jp) != '\0') {
        char c = json_consume(jp);
        if (c == '\\') {
            c = json_consume(jp);
            switch (c) {
                case 'n':  c = '\n'; break;
                case 'r':  c = '\r'; break;
                case 't':  c = '\t'; break;
                case '"':  c = '"'; break;
                case '\\': c = '\\'; break;
                case '/':  c = '/'; break;
                default: break;
            }
        }
        if (remaining > 0) {
            *buf++ = c;
            remaining--;
        }
    }

    if (json_consume(jp) != '"') {
        jp->error = strdup("Expected closing '\"'");
        return NULL;
    }

    *buf = '\0';
    return strdup(buffer);
}

static ToonValue *parse_json_number(JsonParser *jp) {
    char buffer[64];
    char *buf = buffer;
    size_t remaining = sizeof(buffer) - 1;

    if (json_peek(jp) == '-') {
        *buf++ = json_consume(jp);
        remaining--;
    }

    while (remaining > 0 && (isdigit(json_peek(jp)) || json_peek(jp) == '.' ||
           json_peek(jp) == 'e' || json_peek(jp) == 'E' || json_peek(jp) == '+' || json_peek(jp) == '-')) {
        *buf++ = json_consume(jp);
        remaining--;
    }

    *buf = '\0';

    ToonValue *value = toon_value_create(TOON_NUMBER);
    if (value) {
        value->value.number = strtod(buffer, NULL);
    }

    return value;
}

static ToonValue *parse_json_array(JsonParser *jp) {
    if (json_consume(jp) != '[') {
        jp->error = strdup("Expected '['");
        return NULL;
    }

    skip_json_whitespace(jp);

    ToonValue **elements = malloc(sizeof(ToonValue *) * 256);
    size_t length = 0;

    while (json_peek(jp) != ']' && json_peek(jp) != '\0' && length < 256) {
        elements[length++] = parse_json_value(jp);
        skip_json_whitespace(jp);

        if (json_peek(jp) == ',') {
            json_consume(jp);
            skip_json_whitespace(jp);
        }
    }

    if (json_consume(jp) != ']') {
        jp->error = strdup("Expected ']'");
        for (size_t i = 0; i < length; i++) {
            toon_value_free(elements[i]);
        }
        free(elements);
        return NULL;
    }

    // Check if this should be a tabular array (array of uniform objects)
    bool is_tabular = length > 0 && elements[0]->type == TOON_OBJECT;
    if (is_tabular && length > 1) {
        // Check if all objects have the same keys
        size_t first_len = elements[0]->value.object.length;
        for (size_t i = 1; i < length; i++) {
            if (elements[i]->type != TOON_OBJECT ||
                elements[i]->value.object.length != first_len) {
                is_tabular = false;
                break;
            }
        }

        // Convert to tabular array if uniform
        if (is_tabular && first_len > 0) {
            ToonValue *tabular = toon_value_create(TOON_TABULAR_ARRAY);

            // Copy headers from first object
            tabular->value.tabular.num_headers = first_len;
            tabular->value.tabular.headers = malloc(sizeof(char *) * first_len);
            for (size_t i = 0; i < first_len; i++) {
                tabular->value.tabular.headers[i] = strdup(elements[0]->value.object.entries[i].key);
            }

            // Copy values into rows
            tabular->value.tabular.num_rows = length;
            tabular->value.tabular.rows = malloc(sizeof(ToonValue **) * length);
            for (size_t row = 0; row < length; row++) {
                tabular->value.tabular.rows[row] = malloc(sizeof(ToonValue *) * first_len);
                for (size_t col = 0; col < first_len; col++) {
                    // Transfer ownership of the value
                    tabular->value.tabular.rows[row][col] = elements[row]->value.object.entries[col].value;
                    elements[row]->value.object.entries[col].value = NULL;
                }
            }

            // Free original array elements (shallow)
            for (size_t i = 0; i < length; i++) {
                toon_value_free(elements[i]);
            }
            free(elements);

            return tabular;
        }
    }

    ToonValue *value = toon_value_create(TOON_ARRAY);
    value->value.array.elements = elements;
    value->value.array.length = length;

    return value;
}

static ToonValue *parse_json_object(JsonParser *jp) {
    if (json_consume(jp) != '{') {
        jp->error = strdup("Expected '{'");
        return NULL;
    }

    skip_json_whitespace(jp);

    ToonObjectEntry *entries = malloc(sizeof(ToonObjectEntry) * 256);
    size_t length = 0;

    while (json_peek(jp) != '}' && json_peek(jp) != '\0' && length < 256) {
        skip_json_whitespace(jp);

        // Parse key
        char *key = parse_json_string(jp);
        if (!key) {
            for (size_t i = 0; i < length; i++) {
                free(entries[i].key);
                toon_value_free(entries[i].value);
            }
            free(entries);
            return NULL;
        }

        skip_json_whitespace(jp);

        if (json_consume(jp) != ':') {
            jp->error = strdup("Expected ':'");
            free(key);
            for (size_t i = 0; i < length; i++) {
                free(entries[i].key);
                toon_value_free(entries[i].value);
            }
            free(entries);
            return NULL;
        }

        skip_json_whitespace(jp);

        // Parse value
        ToonValue *val = parse_json_value(jp);

        entries[length].key = key;
        entries[length].value = val;
        length++;

        skip_json_whitespace(jp);

        if (json_peek(jp) == ',') {
            json_consume(jp);
            skip_json_whitespace(jp);
        }
    }

    if (json_consume(jp) != '}') {
        jp->error = strdup("Expected '}'");
        for (size_t i = 0; i < length; i++) {
            free(entries[i].key);
            toon_value_free(entries[i].value);
        }
        free(entries);
        return NULL;
    }

    ToonValue *value = toon_value_create(TOON_OBJECT);
    value->value.object.entries = entries;
    value->value.object.length = length;

    return value;
}

static ToonValue *parse_json_value(JsonParser *jp) {
    skip_json_whitespace(jp);

    char c = json_peek(jp);

    if (c == '"') {
        char *str = parse_json_string(jp);
        ToonValue *value = toon_value_create(TOON_STRING);
        value->value.string = str;
        return value;
    } else if (c == '{') {
        return parse_json_object(jp);
    } else if (c == '[') {
        return parse_json_array(jp);
    } else if (json_match(jp, "true")) {
        ToonValue *value = toon_value_create(TOON_BOOLEAN);
        value->value.boolean = true;
        return value;
    } else if (json_match(jp, "false")) {
        ToonValue *value = toon_value_create(TOON_BOOLEAN);
        value->value.boolean = false;
        return value;
    } else if (json_match(jp, "null")) {
        return toon_value_create(TOON_NULL);
    } else if (isdigit(c) || c == '-') {
        return parse_json_number(jp);
    }

    jp->error = strdup("Unexpected character");
    return NULL;
}

// Public JSON to TOON conversion
ToonValue *json_to_toon(const char *json_string, char **error) {
    JsonParser jp = {
        .json = json_string,
        .current = json_string,
        .error = NULL
    };

    ToonValue *result = parse_json_value(&jp);

    if (jp.error) {
        *error = jp.error;
        if (result) toon_value_free(result);
        return NULL;
    }

    return result;
}
