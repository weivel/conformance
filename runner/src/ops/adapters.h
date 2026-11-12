#ifndef CONFORMANCE_OPS_ADAPTERS_H
#define CONFORMANCE_OPS_ADAPTERS_H

#include "ops.h"

int adapter_supports(const json_value *given, answer_writer *answer);
int adapter_boot(const json_value *given, answer_writer *answer);
int adapter_order(const json_value *given, answer_writer *answer);
int adapter_resolve(const json_value *given, answer_writer *answer);
int adapter_unbound(const json_value *given, answer_writer *answer);
int adapter_carry(const json_value *given, answer_writer *answer);
int adapter_revoke(const json_value *given, answer_writer *answer);
int adapter_scope(const json_value *given, answer_writer *answer);
int adapter_want(const json_value *given, answer_writer *answer);
int adapter_reach(const json_value *given, answer_writer *answer);
int adapter_consume(const json_value *given, answer_writer *answer);

#endif
