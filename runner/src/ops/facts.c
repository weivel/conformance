#include "facts.h"

#include <stdint.h>

static int is_absent(const json_value *value) {
    return value == NULL || json_kind_of(value) == JSON_NULL;
}

static int list_length(const json_value *array, uint32_t *length) {
    size_t count;
    if (is_absent(array)) {
        *length = 0;
        return 1;
    }
    if (json_kind_of(array) != JSON_ARRAY) {
        return 0;
    }
    count = json_array_length(array);
    if (count > UINT32_MAX) {
        return 0;
    }
    *length = (uint32_t)count;
    return 1;
}

static int optional_string(const json_value *value, const char **text) {
    if (is_absent(value)) {
        *text = NULL;
        return 1;
    }
    *text = json_as_string(value);
    return *text != NULL;
}

static int required_string(const json_value *value, const char **text) {
    *text = json_as_string(value);
    return *text != NULL;
}

int facts_strings(arena *owner, const json_value *array, weivel_strings *strings) {
    uint32_t count;
    const char **items;
    uint32_t at;
    strings->items = NULL;
    strings->count = 0;
    if (!list_length(array, &count)) {
        return 0;
    }
    if (count == 0) {
        return 1;
    }
    items = arena_take(owner, count, sizeof *items);
    if (items == NULL) {
        return 0;
    }
    for (at = 0; at < count; at++) {
        if (!required_string(json_array_at(array, at), &items[at])) {
            return 0;
        }
    }
    strings->items = items;
    strings->count = count;
    return 1;
}

int facts_numbers(arena *owner, const json_value *object, weivel_numbers *numbers) {
    size_t count;
    weivel_number *items;
    size_t at;
    numbers->items = NULL;
    numbers->count = 0;
    if (is_absent(object)) {
        return 1;
    }
    if (json_kind_of(object) != JSON_OBJECT) {
        return 0;
    }
    count = json_object_size(object);
    if (count == 0) {
        return 1;
    }
    if (count > UINT32_MAX) {
        return 0;
    }
    items = arena_take(owner, count, sizeof *items);
    if (items == NULL) {
        return 0;
    }
    for (at = 0; at < count; at++) {
        items[at].size = (uint32_t)sizeof items[at];
        items[at].name = json_object_key_at(object, at);
        if (!json_as_double(json_object_value_at(object, at), &items[at].value)) {
            return 0;
        }
    }
    numbers->items = items;
    numbers->count = (uint32_t)count;
    return 1;
}

int facts_choice(const json_value *flag, uint32_t *choice) {
    int value = 0;
    if (is_absent(flag)) {
        *choice = WEIVEL_CHOICE_DEFAULT;
        return 1;
    }
    if (!json_as_bool(flag, &value)) {
        return 0;
    }
    *choice = value ? WEIVEL_CHOICE_YES : WEIVEL_CHOICE_NO;
    return 1;
}

static int service_want(arena *owner, const json_value *entry, weivel_service_want *want) {
    want->size = (uint32_t)sizeof *want;
    if (json_kind_of(entry) == JSON_STRING) {
        want->id = json_as_string(entry);
        return 1;
    }
    return json_kind_of(entry) == JSON_OBJECT && required_string(json_object_get(entry, "id"), &want->id) &&
           optional_string(json_object_get(entry, "provider"), &want->provider) &&
           facts_choice(json_object_get(entry, "optional"), &want->optional) &&
           facts_choice(json_object_get(entry, "degradable"), &want->degradable) &&
           facts_numbers(owner, json_object_get(entry, "constraints"), &want->constraints);
}

int facts_service_wants(arena *owner, const json_value *array, weivel_service_wants *wants) {
    uint32_t count;
    weivel_service_want *items;
    uint32_t at;
    wants->items = NULL;
    wants->count = 0;
    if (!list_length(array, &count)) {
        return 0;
    }
    if (count == 0) {
        return 1;
    }
    items = arena_take(owner, count, sizeof *items);
    if (items == NULL) {
        return 0;
    }
    for (at = 0; at < count; at++) {
        if (!service_want(owner, json_array_at(array, at), &items[at])) {
            return 0;
        }
    }
    wants->items = items;
    wants->count = count;
    return 1;
}

static int service_offer(arena *owner, const json_value *entry, weivel_service_offer *offer) {
    offer->size = (uint32_t)sizeof *offer;
    if (json_kind_of(entry) == JSON_STRING) {
        offer->id = json_as_string(entry);
        return 1;
    }
    return json_kind_of(entry) == JSON_OBJECT && required_string(json_object_get(entry, "id"), &offer->id) &&
           facts_numbers(owner, json_object_get(entry, "attributes"), &offer->attributes);
}

int facts_service_offers(arena *owner, const json_value *array, weivel_service_offers *offers) {
    uint32_t count;
    weivel_service_offer *items;
    uint32_t at;
    offers->items = NULL;
    offers->count = 0;
    if (!list_length(array, &count)) {
        return 0;
    }
    if (count == 0) {
        return 1;
    }
    items = arena_take(owner, count, sizeof *items);
    if (items == NULL) {
        return 0;
    }
    for (at = 0; at < count; at++) {
        if (!service_offer(owner, json_array_at(array, at), &items[at])) {
            return 0;
        }
    }
    offers->items = items;
    offers->count = count;
    return 1;
}

static int api_level(const json_value *value, uint32_t *api) {
    int64_t level = 0;
    if (is_absent(value)) {
        *api = 0;
        return 1;
    }
    if (!json_as_int64(value, &level) || level < 0 || level > UINT32_MAX) {
        return 0;
    }
    *api = (uint32_t)level;
    return 1;
}

static int import_name(const json_value *object, const char **name) {
    const json_value *named = json_object_get(object, "importName");
    return optional_string(is_absent(named) ? json_object_get(object, "id") : named, name);
}

static int capability_declarations(arena *owner, const json_value *object, weivel_capability_declarations *declared) {
    return facts_strings(owner, json_object_get(object, "provides"), &declared->provides) &&
           facts_strings(owner, json_object_get(object, "consumes"), &declared->consumes);
}

static int service_declarations(arena *owner, const json_value *object, weivel_service_declarations *declared) {
    return facts_service_offers(owner, json_object_get(object, "provides"), &declared->provides) &&
           facts_service_wants(owner, json_object_get(object, "consumes"), &declared->consumes);
}

int facts_manifest(arena *owner, const json_value *object, weivel_manifest *manifest) {
    weivel_manifest built = WEIVEL_INIT(weivel_manifest);
    *manifest = built;
    if (object == NULL || json_kind_of(object) != JSON_OBJECT) {
        return 0;
    }
    return import_name(object, &manifest->import_name) && api_level(json_object_get(object, "api"), &manifest->api) &&
           optional_string(json_object_get(object, "entry"), &manifest->entry) &&
           optional_string(json_object_get(object, "identity"), &manifest->identity) &&
           facts_strings(owner, json_object_get(object, "targets"), &manifest->targets) &&
           facts_strings(owner, json_object_get(object, "permissions"), &manifest->permissions) &&
           capability_declarations(owner, json_object_get(object, "capabilities"), &manifest->capabilities) &&
           service_declarations(owner, json_object_get(object, "services"), &manifest->services);
}
