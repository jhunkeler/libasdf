#include <libfyaml.h>

#include "event.h"
#include "yaml.h"


static const asdf_yaml_event_type_t fyet_to_asdf_event[] = {
    [FYET_STREAM_START] = ASDF_YAML_STREAM_START_EVENT,
    [FYET_STREAM_END] = ASDF_YAML_STREAM_END_EVENT,
    [FYET_DOCUMENT_START] = ASDF_YAML_DOCUMENT_START_EVENT,
    [FYET_DOCUMENT_END] = ASDF_YAML_DOCUMENT_END_EVENT,
    [FYET_MAPPING_START] = ASDF_YAML_MAPPING_START_EVENT,
    [FYET_MAPPING_END] = ASDF_YAML_MAPPING_END_EVENT,
    [FYET_SEQUENCE_START] = ASDF_YAML_SEQUENCE_START_EVENT,
    [FYET_SEQUENCE_END] = ASDF_YAML_SEQUENCE_END_EVENT,
    [FYET_SCALAR] = ASDF_YAML_SCALAR_EVENT,
    [FYET_ALIAS] = ASDF_YAML_ALIAS_EVENT,
};


#define ASDF_IS_YAML_EVENT(event) \
    ((event) && (event)->type == ASDF_YAML_EVENT && (event)->payload.yaml)


asdf_yaml_event_type_t asdf_yaml_event_type(const asdf_event_t *event) {
    if (!ASDF_IS_YAML_EVENT(event))
        return ASDF_YAML_NONE_EVENT;

    enum fy_event_type type = event->payload.yaml->type;
    if (type < 0 || type >= (int)(sizeof(fyet_to_asdf_event) / sizeof(fyet_to_asdf_event[0]))) {
        abort();
    }
    return fyet_to_asdf_event[type];
}

/**
 * Return a text representation of a YAML event type
 */
const char *asdf_yaml_event_type_text(const asdf_event_t *event) {
    if (!ASDF_IS_YAML_EVENT(event))
        return "";

    return fy_event_type_get_text(event->payload.yaml->type);
}


/**
 * Return unparsed YAML scalar value associated with an event, if any
 *
 * Returns NULL if the event is not a YAML scalar event
 */
const char *asdf_yaml_event_scalar_value(const asdf_event_t *event, size_t *lenp) {
    if (!ASDF_IS_YAML_EVENT(event))
        return NULL;

    if (event->payload.yaml->type != FYET_SCALAR) {
        *lenp = 0;
        return NULL;
    }

    struct fy_token *token = event->payload.yaml->scalar.value;
    // Is safe to call if there is no token, just returns empty string/0
    return fy_token_get_text(token, lenp);
}


/**
 * Return the YAML tag associated with an event, if any
 *
 * Returns NULL if the event is not a YAML event or if there is no tag
 */
const char *asdf_yaml_event_tag(const asdf_event_t *event, size_t *lenp) {
    if (!ASDF_IS_YAML_EVENT(event))
        return NULL;

    struct fy_token *token = fy_event_get_tag_token(event->payload.yaml);
    // Is safe to call if there is no token, just returns empty string/0
    return fy_token_get_text(token, lenp);
}
