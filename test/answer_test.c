#include "answer.h"
#include "check.h"

static void writes_nested_containers_with_separators(void) {
    answer_writer writer;
    answer_init(&writer);
    answer_begin_object(&writer);
    answer_key(&writer, "started");
    answer_begin_array(&writer);
    answer_string(&writer, "a");
    answer_string(&writer, "b");
    answer_end_array(&writer);
    answer_key(&writer, "refusal");
    answer_null(&writer);
    answer_key(&writer, "atPoll");
    answer_int(&writer, -2);
    answer_key(&writer, "served");
    answer_bool(&writer, 1);
    answer_end_object(&writer);
    CHECK(answer_is_complete(&writer));
    check_text(answer_text(&writer), "{\"started\":[\"a\",\"b\"],\"refusal\":null,\"atPoll\":-2,\"served\":true}",
               "document");
    answer_release(&writer);
}

static void escapes_what_a_string_may_not_hold_raw(void) {
    answer_writer writer;
    answer_init(&writer);
    answer_string(&writer, "a\"b\\c\n");
    check_text(answer_text(&writer), "\"a\\\"b\\\\c\\u000a\"", "escaped");
    answer_release(&writer);
}

static void an_unclosed_container_is_not_complete(void) {
    answer_writer writer;
    answer_init(&writer);
    answer_begin_object(&writer);
    answer_key(&writer, "started");
    answer_begin_array(&writer);
    CHECK(!answer_is_complete(&writer));
    answer_release(&writer);
}

static void a_key_without_its_value_is_not_complete(void) {
    answer_writer writer;
    answer_init(&writer);
    answer_begin_object(&writer);
    answer_key(&writer, "started");
    answer_end_object(&writer);
    CHECK(!answer_is_complete(&writer));
    answer_release(&writer);
}

static void nothing_written_is_not_complete(void) {
    answer_writer writer;
    answer_init(&writer);
    CHECK(!answer_is_complete(&writer));
    answer_release(&writer);
}

static void a_key_outside_an_object_is_refused(void) {
    answer_writer writer;
    answer_init(&writer);
    answer_begin_object(&writer);
    answer_end_object(&writer);
    CHECK(answer_is_complete(&writer));
    answer_key(&writer, "started");
    CHECK(!answer_is_complete(&writer));
    answer_release(&writer);
}

int main(void) {
    writes_nested_containers_with_separators();
    escapes_what_a_string_may_not_hold_raw();
    an_unclosed_container_is_not_complete();
    a_key_without_its_value_is_not_complete();
    nothing_written_is_not_complete();
    a_key_outside_an_object_is_refused();
    return check_summary("answer_test");
}
