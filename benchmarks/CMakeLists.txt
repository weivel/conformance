add_executable(weivel-benchmark-control-call control_call.c)
target_link_libraries(weivel-benchmark-control-call PRIVATE weivel_core)
conformance_strict_warnings(weivel-benchmark-control-call)
if(NOT MSVC)
    target_link_libraries(weivel-benchmark-control-call PRIVATE m)
endif()

set(benchmark_fixtures ${PROJECT_SOURCE_DIR}/test/fixtures/benchmark)
set(benchmark_script ${PROJECT_SOURCE_DIR}/test/benchmark_test.cmake)

function(benchmark_test name expected_exit mode budget)
    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:weivel-benchmark-control-call> -DMODE=${mode}
            -DBUDGET=${budget} -DEXPECTED_EXIT=${expected_exit} -DCONFIG=$<CONFIG> ${ARGN}
            -P ${benchmark_script})
    set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
endfunction()

benchmark_test(benchmark_measure_prints_the_statistics 0 measure "" -DMUST_MATCH=median_nanoseconds)
benchmark_test(benchmark_gate_exits_over_budget 1 gate ${benchmark_fixtures}/one-nanosecond.txt)
benchmark_test(benchmark_gate_refuses_a_missing_budget 2 gate ${benchmark_fixtures}/no-such-budget.txt)
foreach(fixture not-a-number wrong-key trailing-junk empty infinite)
    benchmark_test(benchmark_gate_refuses_a_budget_that_is_${fixture} 2 gate ${benchmark_fixtures}/${fixture}.txt)
endforeach()
benchmark_test(benchmark_gate_holds_the_committed_budget 0 gate ${CMAKE_CURRENT_SOURCE_DIR}/budget.txt
    -DRELEASE_ONLY=ON)
set_tests_properties(benchmark_gate_holds_the_committed_budget PROPERTIES LABELS release-benchmark)
