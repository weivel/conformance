add_library(conformance_json STATIC src/json.c)
target_include_directories(conformance_json PUBLIC src)
conformance_strict_warnings(conformance_json)

add_library(conformance_runner STATIC
    src/answer.c
    src/match.c
    src/ops/ops.c
    src/report.c
    src/runner.c
    src/text.c
    src/vectors.c
    src/walk.c)
target_link_libraries(conformance_runner PUBLIC conformance_json)
conformance_strict_warnings(conformance_runner)

add_executable(weivel-conformance src/main.c)
target_link_libraries(weivel-conformance PRIVATE conformance_runner)
conformance_strict_warnings(weivel-conformance)
