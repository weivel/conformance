#ifndef CONFORMANCE_OPS_ADAPTERS_H
#define CONFORMANCE_OPS_ADAPTERS_H

#include "ops.h"

int adapter_supports(const weivel_json_value *given, answer_writer *answer);
int adapter_boot(const weivel_json_value *given, answer_writer *answer);
int adapter_order(const weivel_json_value *given, answer_writer *answer);
int adapter_resolve(const weivel_json_value *given, answer_writer *answer);
int adapter_unbound(const weivel_json_value *given, answer_writer *answer);
int adapter_carry(const weivel_json_value *given, answer_writer *answer);
int adapter_revoke(const weivel_json_value *given, answer_writer *answer);
int adapter_scope(const weivel_json_value *given, answer_writer *answer);
int adapter_want(const weivel_json_value *given, answer_writer *answer);
int adapter_reach(const weivel_json_value *given, answer_writer *answer);
int adapter_consume(const weivel_json_value *given, answer_writer *answer);

#endif
