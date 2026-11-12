#ifndef CONFORMANCE_OPS_WANT_STATES_H
#define CONFORMANCE_OPS_WANT_STATES_H

#include "hearing.h"

/**
 * `settled`, `rejected` when the consumer was released (WEIVEL_REFUSAL_STOPPED), or NULL for any other rejection.
 * @implNote A rejection reads as the contract's word only for the code the contract means by it, so a want rejected
 * for an unrelated reason errors the vector rather than passing as the rejection it asserts.
 */
const char *want_state_in_scope(const heard_event *settlement);

/**
 * `served`, `refused` when the fence refused it (WEIVEL_REFUSAL_UNDECLARED or WEIVEL_REFUSAL_WRONG_PROVIDER), or NULL
 * for any other rejection, for the reason `want_state_in_scope` gives.
 */
const char *want_state_in_reach(const heard_event *settlement);

#endif
