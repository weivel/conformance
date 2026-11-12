#include "want_states.h"

const char *want_state_in_scope(const heard_event *settlement) {
    if (settlement->implementation != 0) {
        return "settled";
    }
    return settlement->code == WEIVEL_REFUSAL_STOPPED ? "rejected" : NULL;
}

const char *want_state_in_reach(const heard_event *settlement) {
    if (settlement->implementation != 0) {
        return "served";
    }
    if (settlement->code == WEIVEL_REFUSAL_UNDECLARED || settlement->code == WEIVEL_REFUSAL_WRONG_PROVIDER) {
        return "refused";
    }
    return NULL;
}
