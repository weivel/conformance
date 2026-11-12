#include "answer.h"

#include <string.h>

void answer_init(answer_writer *writer) {
    memset(writer, 0, sizeof *writer);
    text_init(&writer->text);
}

void answer_release(answer_writer *writer) {
    text_release(&writer->text);
}

static void write_separator(answer_writer *writer) {
    if (writer->after_key) {
        writer->after_key = 0;
        return;
    }
    if (writer->has_items[writer->depth]) {
        text_append(&writer->text, ",");
    }
    writer->has_items[writer->depth] = 1;
}

static void write_quoted(answer_writer *writer, const char *value) {
    const unsigned char *at;
    text_append(&writer->text, "\"");
    for (at = (const unsigned char *)value; *at != '\0'; at++) {
        if (*at == '"' || *at == '\\') {
            text_append_format(&writer->text, "\\%c", *at);
        } else if (*at < 0x20) {
            text_append_format(&writer->text, "\\u%04x", *at);
        } else {
            text_append_bytes(&writer->text, (const char *)at, 1);
        }
    }
    text_append(&writer->text, "\"");
}

static void open_container(answer_writer *writer, const char *opener) {
    write_separator(writer);
    if (writer->depth + 1 >= ANSWER_MAXIMUM_DEPTH) {
        writer->broken = 1;
        return;
    }
    text_append(&writer->text, opener);
    writer->depth++;
    writer->has_items[writer->depth] = 0;
}

static void close_container(answer_writer *writer, const char *closer) {
    if (writer->depth == 0 || writer->after_key) {
        writer->broken = 1;
        return;
    }
    writer->depth--;
    text_append(&writer->text, closer);
}

void answer_begin_object(answer_writer *writer) {
    open_container(writer, "{");
}

void answer_end_object(answer_writer *writer) {
    close_container(writer, "}");
}

void answer_begin_array(answer_writer *writer) {
    open_container(writer, "[");
}

void answer_end_array(answer_writer *writer) {
    close_container(writer, "]");
}

void answer_key(answer_writer *writer, const char *key) {
    if (writer->depth == 0 || writer->after_key) {
        writer->broken = 1;
        return;
    }
    write_separator(writer);
    write_quoted(writer, key);
    text_append(&writer->text, ":");
    writer->after_key = 1;
}

void answer_string(answer_writer *writer, const char *value) {
    write_separator(writer);
    write_quoted(writer, value);
}

void answer_int(answer_writer *writer, int64_t value) {
    write_separator(writer);
    text_append_format(&writer->text, "%lld", (long long)value);
}

void answer_bool(answer_writer *writer, int value) {
    write_separator(writer);
    text_append(&writer->text, value ? "true" : "false");
}

void answer_null(answer_writer *writer) {
    write_separator(writer);
    text_append(&writer->text, "null");
}

const char *answer_text(const answer_writer *writer) {
    return text_view(&writer->text);
}

int answer_is_complete(const answer_writer *writer) {
    return !writer->broken && !writer->text.failed && writer->depth == 0 && !writer->after_key &&
           writer->text.length > 0;
}
