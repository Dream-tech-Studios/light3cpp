/*
 * json.h - minimal JSON reader (parse-only, no writer).
 *
 * Hand-rolled and deliberately small: just enough of the JSON grammar
 * (objects, arrays, strings, numbers, true/false/null) to read simple game
 * data files like assets/characters.json, without pulling in a third-party
 * dependency for something this project can implement in ~200 lines - in
 * keeping with the "no bloat" approach used for the OBJ loader and font.
 *
 * Consumers are character.c (on both PLATFORM_PC and PLATFORM_GC - see
 * character.h) and the host-side bake tool (tools/obj2c.c, PLATFORM_PC only).
 */
#ifndef JSON_H
#define JSON_H

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

typedef struct JsonValue JsonValue;

/* Parses a NUL-terminated JSON document. Returns NULL and (optionally)
   fills *out_error with a short message on malformed input. Free the
   result with json_free(). */
JsonValue *json_parse(const char *text, char *out_error, int out_error_size);
void       json_free(JsonValue *v);

JsonType json_type(const JsonValue *v);

int        json_array_count(const JsonValue *v);            /* 0 if not an array */
JsonValue *json_array_get(const JsonValue *v, int index);    /* NULL if out of range */
JsonValue *json_object_get(const JsonValue *v, const char *key); /* NULL if missing/not an object */

const char *json_as_string(const JsonValue *v, const char *fallback);
double      json_as_number(const JsonValue *v, double fallback);
int         json_as_int(const JsonValue *v, int fallback);

#endif /* JSON_H */
