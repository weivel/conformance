# -DPROGRAM=<exe> -DMODE=<measure|gate> -DBUDGET=<file or empty> -DEXPECTED_EXIT=<n> -DCONFIG=<config>
# [-DMUST_MATCH=<regex>] [-DRELEASE_ONLY=ON]
#
# A timing verdict from an unoptimised build means nothing, so a RELEASE_ONLY test skips (exit 77) anywhere else.

if(RELEASE_ONLY AND NOT CONFIG STREQUAL "Release")
    message("SKIP: the committed budget is a Release-build figure and this is a ${CONFIG} build")
    cmake_language(EXIT 77)
endif()

execute_process(COMMAND ${PROGRAM} ${MODE} ${BUDGET}
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
set(output "${out}${err}")
if(NOT code STREQUAL EXPECTED_EXIT)
    message(FATAL_ERROR "exit ${code}, expected ${EXPECTED_EXIT}\n${output}")
endif()
if(DEFINED MUST_MATCH AND NOT output MATCHES "${MUST_MATCH}")
    message(FATAL_ERROR "output does not match '${MUST_MATCH}'\n${output}")
endif()
