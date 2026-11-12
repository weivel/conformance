add_library(conformance_json STATIC src/json.c)
target_include_directories(conformance_json PUBLIC src)
conformance_strict_warnings(conformance_json)

add_library(conformance_runner STATIC
    src/answer.c
    src/match.c
    src/ops/arena.c
    src/ops/carry.c
    src/ops/cast.c
    src/ops/consume.c
    src/ops/facts.c
    src/ops/hearing.c
    src/ops/ops.c
    src/ops/plan.c
    src/ops/reach.c
    src/ops/resolve.c
    src/ops/revoke.c
    src/ops/scope.c
    src/ops/session.c
    src/ops/supports.c
    src/ops/unbound.c
    src/ops/want.c
    src/report.c
    src/runner.c
    src/text.c
    src/vectors.c
    src/walk.c)
target_link_libraries(conformance_runner PUBLIC conformance_json weivel_core)
conformance_strict_warnings(conformance_runner)

add_executable(weivel-conformance src/main.c)
target_link_libraries(weivel-conformance PRIVATE conformance_runner)
conformance_strict_warnings(weivel-conformance)
