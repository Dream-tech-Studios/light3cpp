/*
 * json.c - see json.h. Pure in-memory text parser (no file I/O of its own),
 * used by character.c on both PLATFORM_PC and PLATFORM_GC.
 */
#if defined(PLATFORM_PC) || defined(PLATFORM_GC)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "json.h"

struct JsonValue {
    JsonType type;
    union {
        int    boolean;
        double number;
        char  *string;
        struct { JsonValue **items; int count; }               array;
        struct { char **keys; JsonValue **values; int count; } object;
    } u;
};

typedef struct {
    const char *p;
    char       *err;
    int         err_size;
    int         failed;
} Parser;

static void fail(Parser *ps, const char *msg) {
    if (ps->failed) return; /* keep the first error */
    ps->failed = 1;
    if (ps->err && ps->err_size > 0) {
        strncpy(ps->err, msg, (size_t)ps->err_size - 1);
        ps->err[ps->err_size - 1] = '\0';
    }
}

static void skip_ws(Parser *ps) {
    while (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\n' || *ps->p == '\r') ++ps->p;
}

static JsonValue *value_new(JsonType type) {
    JsonValue *v = (JsonValue *)calloc(1, sizeof(JsonValue));
    v->type = type;
    return v;
}

static JsonValue *parse_value(Parser *ps);

static int parse_hex4(const char *p, unsigned int *out) {
    int i;
    unsigned int v = 0;
    for (i = 0; i < 4; ++i) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned int)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned int)(c - 'A' + 10);
        else return 0;
    }
    *out = v;
    return 1;
}

/* Parses a quoted JSON string starting at *ps->p == '"'. Supports the
   common escapes; \uXXXX is collapsed to '?' for anything outside ASCII
   since none of this project's data files need full Unicode. */
static char *parse_string_raw(Parser *ps) {
    const char *start;
    char *out;
    size_t cap, len = 0;

    if (*ps->p != '"') { fail(ps, "expected string"); return NULL; }
    ++ps->p;
    start = ps->p;
    (void)start;

    cap = 32;
    out = (char *)malloc(cap);

    while (*ps->p && *ps->p != '"') {
        char c = *ps->p;
        if (len + 1 >= cap) { cap *= 2; out = (char *)realloc(out, cap); }

        if (c == '\\') {
            char esc = *(ps->p + 1);
            ++ps->p;
            switch (esc) {
            case '"':  out[len++] = '"';  break;
            case '\\': out[len++] = '\\'; break;
            case '/':  out[len++] = '/';  break;
            case 'b':  out[len++] = '\b'; break;
            case 'f':  out[len++] = '\f'; break;
            case 'n':  out[len++] = '\n'; break;
            case 'r':  out[len++] = '\r'; break;
            case 't':  out[len++] = '\t'; break;
            case 'u': {
                unsigned int cp;
                if (!parse_hex4(ps->p + 1, &cp)) { fail(ps, "bad \\u escape"); free(out); return NULL; }
                ps->p += 4;
                out[len++] = (cp < 0x80) ? (char)cp : '?';
                break;
            }
            default:
                fail(ps, "unknown escape");
                free(out);
                return NULL;
            }
            ++ps->p;
        } else {
            out[len++] = c;
            ++ps->p;
        }
    }

    if (*ps->p != '"') { fail(ps, "unterminated string"); free(out); return NULL; }
    ++ps->p;
    out[len] = '\0';
    return out;
}

static JsonValue *parse_string(Parser *ps) {
    JsonValue *v;
    char *s = parse_string_raw(ps);
    if (!s) return NULL;
    v = value_new(JSON_STRING);
    v->u.string = s;
    return v;
}

static JsonValue *parse_number(Parser *ps) {
    char *end;
    double n = strtod(ps->p, &end);
    JsonValue *v;
    if (end == ps->p) { fail(ps, "bad number"); return NULL; }
    ps->p = end;
    v = value_new(JSON_NUMBER);
    v->u.number = n;
    return v;
}

static int match_literal(Parser *ps, const char *lit) {
    size_t n = strlen(lit);
    if (strncmp(ps->p, lit, n) == 0) { ps->p += n; return 1; }
    return 0;
}

static JsonValue *parse_array(Parser *ps) {
    JsonValue *v = value_new(JSON_ARRAY);
    int cap = 0;
    ++ps->p; /* '[' */
    skip_ws(ps);

    if (*ps->p == ']') { ++ps->p; return v; }

    for (;;) {
        JsonValue *item;
        skip_ws(ps);
        item = parse_value(ps);
        if (ps->failed) { json_free(v); return NULL; }

        if (v->u.array.count == cap) {
            cap = cap ? cap * 2 : 8;
            v->u.array.items = (JsonValue **)realloc(v->u.array.items, sizeof(JsonValue *) * (size_t)cap);
        }
        v->u.array.items[v->u.array.count++] = item;

        skip_ws(ps);
        if (*ps->p == ',') { ++ps->p; continue; }
        if (*ps->p == ']') { ++ps->p; break; }
        fail(ps, "expected ',' or ']' in array");
        json_free(v);
        return NULL;
    }
    return v;
}

static JsonValue *parse_object(Parser *ps) {
    JsonValue *v = value_new(JSON_OBJECT);
    int cap = 0;
    ++ps->p; /* '{' */
    skip_ws(ps);

    if (*ps->p == '}') { ++ps->p; return v; }

    for (;;) {
        char *key;
        JsonValue *val;

        skip_ws(ps);
        key = parse_string_raw(ps);
        if (!key) { json_free(v); return NULL; }

        skip_ws(ps);
        if (*ps->p != ':') { fail(ps, "expected ':' in object"); free(key); json_free(v); return NULL; }
        ++ps->p;
        skip_ws(ps);

        val = parse_value(ps);
        if (ps->failed) { free(key); json_free(v); return NULL; }

        if (v->u.object.count == cap) {
            cap = cap ? cap * 2 : 8;
            v->u.object.keys   = (char **)realloc(v->u.object.keys, sizeof(char *) * (size_t)cap);
            v->u.object.values = (JsonValue **)realloc(v->u.object.values, sizeof(JsonValue *) * (size_t)cap);
        }
        v->u.object.keys[v->u.object.count]   = key;
        v->u.object.values[v->u.object.count] = val;
        ++v->u.object.count;

        skip_ws(ps);
        if (*ps->p == ',') { ++ps->p; continue; }
        if (*ps->p == '}') { ++ps->p; break; }
        fail(ps, "expected ',' or '}' in object");
        json_free(v);
        return NULL;
    }
    return v;
}

static JsonValue *parse_value(Parser *ps) {
    skip_ws(ps);
    switch (*ps->p) {
    case '{': return parse_object(ps);
    case '[': return parse_array(ps);
    case '"': return parse_string(ps);
    case 't': if (match_literal(ps, "true"))  { JsonValue *v = value_new(JSON_BOOL); v->u.boolean = 1; return v; } break;
    case 'f': if (match_literal(ps, "false")) { JsonValue *v = value_new(JSON_BOOL); v->u.boolean = 0; return v; } break;
    case 'n': if (match_literal(ps, "null"))  { return value_new(JSON_NULL); } break;
    default:
        if (*ps->p == '-' || isdigit((unsigned char)*ps->p)) return parse_number(ps);
        break;
    }
    fail(ps, "unexpected character");
    return NULL;
}

JsonValue *json_parse(const char *text, char *out_error, int out_error_size) {
    Parser ps;
    JsonValue *root;

    ps.p = text;
    ps.err = out_error;
    ps.err_size = out_error_size;
    ps.failed = 0;

    root = parse_value(&ps);
    if (ps.failed) {
        if (root) json_free(root);
        return NULL;
    }

    skip_ws(&ps);
    if (*ps.p != '\0') {
        fail(&ps, "trailing data after JSON document");
        json_free(root);
        return NULL;
    }
    return root;
}

void json_free(JsonValue *v) {
    int i;
    if (!v) return;
    switch (v->type) {
    case JSON_STRING:
        free(v->u.string);
        break;
    case JSON_ARRAY:
        for (i = 0; i < v->u.array.count; ++i) json_free(v->u.array.items[i]);
        free(v->u.array.items);
        break;
    case JSON_OBJECT:
        for (i = 0; i < v->u.object.count; ++i) {
            free(v->u.object.keys[i]);
            json_free(v->u.object.values[i]);
        }
        free(v->u.object.keys);
        free(v->u.object.values);
        break;
    default:
        break;
    }
    free(v);
}

JsonType json_type(const JsonValue *v) {
    return v ? v->type : JSON_NULL;
}

int json_array_count(const JsonValue *v) {
    return (v && v->type == JSON_ARRAY) ? v->u.array.count : 0;
}

JsonValue *json_array_get(const JsonValue *v, int index) {
    if (!v || v->type != JSON_ARRAY || index < 0 || index >= v->u.array.count) return NULL;
    return v->u.array.items[index];
}

JsonValue *json_object_get(const JsonValue *v, const char *key) {
    int i;
    if (!v || v->type != JSON_OBJECT) return NULL;
    for (i = 0; i < v->u.object.count; ++i) {
        if (strcmp(v->u.object.keys[i], key) == 0) return v->u.object.values[i];
    }
    return NULL;
}

const char *json_as_string(const JsonValue *v, const char *fallback) {
    return (v && v->type == JSON_STRING) ? v->u.string : fallback;
}

double json_as_number(const JsonValue *v, double fallback) {
    return (v && v->type == JSON_NUMBER) ? v->u.number : fallback;
}

int json_as_int(const JsonValue *v, int fallback) {
    return (v && v->type == JSON_NUMBER) ? (int)v->u.number : fallback;
}

#endif /* PLATFORM_PC || PLATFORM_GC */
