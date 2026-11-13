add_library(conformance_runner STATIC
    src/answer.c
    src/match.c
    src/ops/arena.c
    src/ops/carry.c
    src/ops/cast.c
    src/ops/consume.c
    src/ops/facts.c
    src/ops/hearing.c
    src/ops/identity.c
    src/ops/json_text.c
    src/ops/ops.c
    src/ops/plan.c
    src/ops/reach.c
    src/ops/resolve.c
    src/ops/revoke.c
    src/ops/scope.c
    src/ops/session.c
    src/ops/standard_refusal.c
    src/ops/supports.c
    src/ops/unbound.c
    src/ops/validate.c
    src/ops/want.c
    src/ops/want_states.c
    src/report.c
    src/runner.c
    src/text.c
    src/vectors.c
    src/walk.c)
target_link_libraries(conformance_runner PUBLIC weivel_standard weivel_core)
target_include_directories(conformance_runner PUBLIC src)
conformance_strict_warnings(conformance_runner)

add_executable(weivel-conformance src/main.c)
target_link_libraries(weivel-conformance PRIVATE conformance_runner)
conformance_strict_warnings(weivel-conformance)
