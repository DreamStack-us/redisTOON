#include "redistoon.h"
#include <ctype.h>

// Parser state
typedef struct {
    const char *input;
    const char *current;
    int line;
    int column;
    char *error;
} Parser;

// Helper function to set parser error
static void set_error(Parser *p, const char *message) {
    if (p->error) return;  // Already have an error

    size_t len = strlen(message) + 100;
    p->error = malloc(len);
    if (p->error) {
        snprintf(p->error, len, "Line %d, Column %d: %s", p->line, p->column, message);
    }
}

// Skip whitespace and comments
static void skip_whitespace(Parser *p) {
    while (*p->current && isspace(*p->current)) {
        if (*p->current == '\n') {
            p->line++;
            p->column = 0;
        }
        p->current++;
        p->column++;
    }
}

// Check if current position matches a string
static bool match(Parser *p, const char *str) {
    size_t len = strlen(str);
    if (strncmp(p->current, str, len) == 0) {
        p->current += len;
        p->column += len;
        return true;
    }
    return false;
}

// Peek at current character
static char peek(Parser *p) {
    return *p->current;
}

// Consume and return current character
static char consume(Parser *p) {
    char c = *p->current;
    if (c) {
        p->current++;
        p->column++;
        if (c == '\n') {
            p->line++;
            p->column = 0;
        }
    }
    return c;
}

// Parse a quoted string
static char *parse_quoted_string(Parser *p) {
    if (consume(p) != '"') {
        set_error(p, "Expected opening quote");
        return NULL;
    }

    char buffer[4096];
    char *buf = buffer;
    size_t remaining = sizeof(buffer) - 1;

    while (peek(p) != '"' && peek(p) != '\0') {
        char c = consume(p);
        if (c == '\\') {
            c = consume(p);
            switch (c) {
                case 'n':  c = '\n'; break;
                case 'r':  c = '\r'; break;
                case 't':  c = '\t'; break;
                case '"':  c = '"'; break;
                case '\\': c = '\\'; break;
                default:
                    set_error(p, "Invalid escape sequence");
                    return NULL;
            }
        }
        if (remaining > 0) {
            *buf++ = c;
            remaining--;
        }
    }

    if (consume(p) != '"') {
        set_error(p, "Expected closing quote");
        return NULL;
    }

    *buf = '\0';
    return strdup(buffer);
}

// Parse an unquoted string (until delimiter)
static char *parse_unquoted_string(Parser *p, const char *delimiters) {
    char buffer[4096];
    char *buf = buffer;
    size_t remaining = sizeof(buffer) - 1;

    while (peek(p) && !strchr(delimiters, peek(p))) {
        if (remaining > 0) {
            *buf++ = consume(p);
            remaining--;
        } else {
            consume(p);
        }
    }

    *buf = '\0';

    // Trim trailing whitespace
    buf--;
    while (buf >= buffer && isspace(*buf)) {
        *buf-- = '\0';
    }

    return strdup(buffer);
}

// Forward declaration
static ToonValue *parse_value(Parser *p);

// Parse a number
static ToonValue *parse_number(Parser *p) {
    char buffer[64];
    char *buf = buffer;
    size_t remaining = sizeof(buffer) - 1;

    // Handle negative numbers
    if (peek(p) == '-') {
        *buf++ = consume(p);
        remaining--;
    }

    // Parse digits
    while (remaining > 0 && (isdigit(peek(p)) || peek(p) == '.')) {
        *buf++ = consume(p);
        remaining--;
    }

    *buf = '\0';

    ToonValue *value = toon_value_create(TOON_NUMBER);
    if (value) {
        value->value.number = strtod(buffer, NULL);
    }

    return value;
}

// Parse a tabular array
static ToonValue *parse_tabular_array(Parser *p) {
    // Format: [N,]{header1,header2,...}:
    if (consume(p) != '[') {
        set_error(p, "Expected '['");
        return NULL;
    }

    // Parse row count
    char count_buf[32];
    int i = 0;
    while (isdigit(peek(p)) && i < 31) {
        count_buf[i++] = consume(p);
    }
    count_buf[i] = '\0';
    size_t num_rows = atoi(count_buf);

    if (consume(p) != ',') {
        set_error(p, "Expected ','");
        return NULL;
    }

    if (consume(p) != ']') {
        set_error(p, "Expected ']'");
        return NULL;
    }

    if (consume(p) != '{') {
        set_error(p, "Expected '{'");
        return NULL;
    }

    // Parse headers
    char **headers = malloc(sizeof(char *) * 256);
    size_t num_headers = 0;

    while (peek(p) != '}' && num_headers < 256) {
        skip_whitespace(p);

        // Parse header name
        char header_buf[256];
        int j = 0;
        while (peek(p) != ',' && peek(p) != '}' && j < 255) {
            header_buf[j++] = consume(p);
        }
        header_buf[j] = '\0';

        // Trim whitespace
        char *start = header_buf;
        while (isspace(*start)) start++;
        char *end = start + strlen(start) - 1;
        while (end > start && isspace(*end)) *end-- = '\0';

        headers[num_headers++] = strdup(start);

        skip_whitespace(p);
        if (peek(p) == ',') consume(p);
    }

    if (consume(p) != '}') {
        set_error(p, "Expected '}'");
        for (size_t i = 0; i < num_headers; i++) free(headers[i]);
        free(headers);
        return NULL;
    }

    if (consume(p) != ':') {
        set_error(p, "Expected ':'");
        for (size_t i = 0; i < num_headers; i++) free(headers[i]);
        free(headers);
        return NULL;
    }

    skip_whitespace(p);

    // Parse rows
    ToonValue ***rows = malloc(sizeof(ToonValue **) * num_rows);
    for (size_t row = 0; row < num_rows; row++) {
        skip_whitespace(p);
        rows[row] = malloc(sizeof(ToonValue *) * num_headers);

        for (size_t col = 0; col < num_headers; col++) {
            skip_whitespace(p);

            // Parse cell value
            if (peek(p) == '"') {
                char *str = parse_quoted_string(p);
                rows[row][col] = toon_value_create(TOON_STRING);
                rows[row][col]->value.string = str;
            } else if (isdigit(peek(p)) || peek(p) == '-') {
                rows[row][col] = parse_number(p);
            } else {
                char *str = parse_unquoted_string(p, ",\n\r");

                // Check for keywords
                if (strcmp(str, "null") == 0) {
                    free(str);
                    rows[row][col] = toon_value_create(TOON_NULL);
                } else if (strcmp(str, "true") == 0) {
                    free(str);
                    rows[row][col] = toon_value_create(TOON_BOOLEAN);
                    rows[row][col]->value.boolean = true;
                } else if (strcmp(str, "false") == 0) {
                    free(str);
                    rows[row][col] = toon_value_create(TOON_BOOLEAN);
                    rows[row][col]->value.boolean = false;
                } else {
                    rows[row][col] = toon_value_create(TOON_STRING);
                    rows[row][col]->value.string = str;
                }
            }

            skip_whitespace(p);
            if (col < num_headers - 1 && peek(p) == ',') consume(p);
        }

        skip_whitespace(p);
    }

    // Create tabular array value
    ToonValue *value = toon_value_create(TOON_TABULAR_ARRAY);
    value->value.tabular.headers = headers;
    value->value.tabular.num_headers = num_headers;
    value->value.tabular.rows = rows;
    value->value.tabular.num_rows = num_rows;

    return value;
}

// Parse a simple array
static ToonValue *parse_simple_array(Parser *p) {
    // Format: [N]: val1,val2,val3
    if (consume(p) != '[') {
        set_error(p, "Expected '['");
        return NULL;
    }

    char count_buf[32];
    int i = 0;
    while (isdigit(peek(p)) && i < 31) {
        count_buf[i++] = consume(p);
    }
    count_buf[i] = '\0';
    size_t length = atoi(count_buf);

    if (consume(p) != ']') {
        set_error(p, "Expected ']'");
        return NULL;
    }

    if (consume(p) != ':') {
        set_error(p, "Expected ':'");
        return NULL;
    }

    skip_whitespace(p);

    ToonValue **elements = malloc(sizeof(ToonValue *) * length);
    for (size_t j = 0; j < length; j++) {
        skip_whitespace(p);
        elements[j] = parse_value(p);
        skip_whitespace(p);
        if (j < length - 1 && peek(p) == ',') consume(p);
    }

    ToonValue *value = toon_value_create(TOON_ARRAY);
    value->value.array.elements = elements;
    value->value.array.length = length;

    return value;
}

// Parse an object
static ToonValue *parse_object(Parser *p) {
    ToonObjectEntry *entries = malloc(sizeof(ToonObjectEntry) * 256);
    size_t length = 0;

    while (peek(p) != '\0' && length < 256) {
        skip_whitespace(p);

        // Check for end of object
        if (peek(p) == '\0') break;

        // Parse key
        char key_buf[256];
        int i = 0;
        while (peek(p) != ':' && peek(p) != '\0' && i < 255) {
            key_buf[i++] = consume(p);
        }
        key_buf[i] = '\0';

        // Trim whitespace from key
        char *start = key_buf;
        while (isspace(*start)) start++;
        char *end = start + strlen(start) - 1;
        while (end > start && isspace(*end)) *end-- = '\0';

        if (peek(p) != ':') break;
        consume(p);  // Consume ':'

        skip_whitespace(p);

        // Parse value
        ToonValue *val = parse_value(p);

        entries[length].key = strdup(start);
        entries[length].value = val;
        length++;

        skip_whitespace(p);
        if (peek(p) == '\n') consume(p);
    }

    ToonValue *value = toon_value_create(TOON_OBJECT);
    value->value.object.entries = entries;
    value->value.object.length = length;

    return value;
}

// Parse any value
static ToonValue *parse_value(Parser *p) {
    skip_whitespace(p);

    char c = peek(p);

    // Quoted string
    if (c == '"') {
        char *str = parse_quoted_string(p);
        ToonValue *value = toon_value_create(TOON_STRING);
        value->value.string = str;
        return value;
    }

    // Number
    if (isdigit(c) || c == '-') {
        return parse_number(p);
    }

    // Array (tabular or simple)
    if (c == '[') {
        // Peek ahead to determine array type
        const char *lookahead = p->current + 1;
        while (isdigit(*lookahead)) lookahead++;

        if (*lookahead == ',') {
            return parse_tabular_array(p);
        } else {
            return parse_simple_array(p);
        }
    }

    // Unquoted string or keyword
    char *str = parse_unquoted_string(p, ",\n\r:");

    if (strcmp(str, "null") == 0) {
        free(str);
        return toon_value_create(TOON_NULL);
    } else if (strcmp(str, "true") == 0) {
        free(str);
        ToonValue *value = toon_value_create(TOON_BOOLEAN);
        value->value.boolean = true;
        return value;
    } else if (strcmp(str, "false") == 0) {
        free(str);
        ToonValue *value = toon_value_create(TOON_BOOLEAN);
        value->value.boolean = false;
        return value;
    }

    ToonValue *value = toon_value_create(TOON_STRING);
    value->value.string = str;
    return value;
}

// Public decoding function
ToonValue *toon_decode(const char *toon_string, char **error) {
    Parser p = {
        .input = toon_string,
        .current = toon_string,
        .line = 1,
        .column = 0,
        .error = NULL
    };

    skip_whitespace(&p);

    // Determine if this is an object or a single value
    ToonValue *result;

    // Check if it looks like an object (key: value format)
    const char *lookahead = p.current;
    bool looks_like_object = false;
    while (*lookahead && *lookahead != '\n') {
        if (*lookahead == ':' && *(lookahead - 1) != ']') {
            looks_like_object = true;
            break;
        }
        lookahead++;
    }

    if (looks_like_object) {
        result = parse_object(&p);
    } else {
        result = parse_value(&p);
    }

    if (p.error) {
        *error = p.error;
        if (result) toon_value_free(result);
        return NULL;
    }

    return result;
}
