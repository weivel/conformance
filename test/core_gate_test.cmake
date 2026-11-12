# -DSCENARIO=<name> -DPLATFORM=<linux|macos|windows> -DCURRENT=<the platform this runs on> -DGATE=<exe> -DRULES=<file>
# -DFORMAT=<msvc|gnu|apple> -DTOOL=<dumpbin or nm> -DSILENT=<a tool that prints nothing> -DSHARED=<fixture library>
# -DOBJECTS=<the same fixture as a static library>
#
# A scenario written for another platform exits 77, which ctest reports as a skip rather than a pass. Exiting with a
# chosen code needs CMake 3.29; an older CMake fails the scenario instead, so it can never pass unseen.

if(NOT PLATFORM STREQUAL CURRENT)
    message("not on this platform: ${SCENARIO} needs ${PLATFORM}, and this is ${CURRENT}")
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.29)
        cmake_language(EXIT 77)
    endif()
    message(FATAL_ERROR "this CMake cannot report a skip")
endif()

function(expect expected_exit must_match must_not_match)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(output "${out}${err}")
    if(NOT code STREQUAL expected_exit)
        message(FATAL_ERROR "${SCENARIO}: exit ${code}, expected ${expected_exit}\n${output}")
    endif()
    if(NOT must_match STREQUAL "" AND NOT output MATCHES "${must_match}")
        message(FATAL_ERROR "${SCENARIO}: output does not match '${must_match}'\n${output}")
    endif()
    if(NOT must_not_match STREQUAL "" AND output MATCHES "${must_not_match}")
        message(FATAL_ERROR "${SCENARIO}: output matches '${must_not_match}'\n${output}")
    endif()
    message("${output}")
endfunction()

set(the_shared_library ${GATE} ${RULES} ${FORMAT} ${TOOL} ${SHARED})
set(the_shared_library_and_its_objects ${the_shared_library} ${OBJECTS})
if(PLATFORM STREQUAL "windows")
    set(lock_call AcquireSRWLockExclusive)
    set(exit_without_objects 1)
else()
    set(lock_call pthread_mutex_lock)
    set(exit_without_objects 0)
endif()

if(SCENARIO STREQUAL "accepts_a_clean_fixture")
    expect(0 "the core reads nothing of the world" "not on the allow list" ${the_shared_library_and_its_objects})
elseif(SCENARIO STREQUAL "refuses_getenv")
    expect(1 "getenv, imported from [^\n]* reads the world \\(environment\\)" "" ${the_shared_library})
    expect(1 "plant_getenv[^:\n]*: getenv reads the world \\(environment\\)" "" ${the_shared_library_and_its_objects})
elseif(SCENARIO STREQUAL "refuses_fopen")
    expect(1 "fopen, imported from [^\n]* reads the world \\(file-access\\)" "" ${the_shared_library})
    expect(1 "plant_fopen[^:\n]*: fopen reads the world \\(file-access\\)" "" ${the_shared_library_and_its_objects})
elseif(SCENARIO STREQUAL "refuses_time")
    expect(1 "time(64)?, imported from [^\n]* reads the world \\(clock\\)" "" ${the_shared_library})
    expect(1 "plant_time[^:\n]*: _?time(64)? reads the world \\(clock\\)" "" ${the_shared_library_and_its_objects})
elseif(SCENARIO STREQUAL "refuses_an_import_off_the_allow_list")
    expect(1 "qsort, imported from [^\n]* is not on the allow list" "qsort, imported from [^\n]*reads the world" ${the_shared_library})
    expect(1 "plant_qsort[^:\n]*: qsort is not on the allow list" "" ${the_shared_library_and_its_objects})
elseif(SCENARIO STREQUAL "refuses_a_parser_it_imports")
    expect(1 "json_parse_document carries a forbidden name \\(json\\)" "json_parse_document[^\n]*allow list"
        ${the_shared_library})
    expect(1 "plant_parser_import[^:\n]*: json_parse_document carries a forbidden name \\(json\\)" ""
        ${the_shared_library_and_its_objects})
elseif(SCENARIO STREQUAL "refuses_a_parser_inside_it")
    expect(1 "fixture_json_parse carries a forbidden name \\(json\\)" "" ${the_shared_library})
    expect(1 "plant_parser_inside[^:\n]*: fixture_json_parse carries a forbidden name \\(json\\)" ""
        ${the_shared_library_and_its_objects})
elseif(SCENARIO STREQUAL "refuses_a_lock_call_outside_the_mutex")
    expect(1 "plant_lock_elsewhere[^:\n]*: ${lock_call} is a lock call outside the mutex object, [^\n]*mutex"
        "mutex\\.(obj|c\\.o)[)]?: ${lock_call}" ${the_shared_library_and_its_objects})
elseif(SCENARIO STREQUAL "says_where_lock_calls_cannot_be_attributed")
    expect(${exit_without_objects}
        "lock calls [^\n]*${lock_call}[^\n]*cannot say which object makes them, and no objects were given, so they are NOT attributed"
        "lock call outside" ${the_shared_library})
elseif(SCENARIO STREQUAL "clears_a_startup_clock_only_through_the_objects")
    expect(1 "QueryPerformanceCounter, imported from KERNEL32.dll, reads the world \\(clock\\); the msvc-dll startup code draws it in, but without the core's objects nothing proves"
        "" ${the_shared_library})
    expect(0 "QueryPerformanceCounter, imported from KERNEL32.dll, reads the world \\(clock\\), but only the msvc-dll startup code calls it"
        "" ${the_shared_library_and_its_objects})
elseif(SCENARIO STREQUAL "refuses_a_tool_that_reads_nothing")
    expect(2 "no imports read from [^\n]*so nothing was checked" "reads nothing of the world"
        ${GATE} ${RULES} ${FORMAT} ${SILENT} ${SHARED} ${OBJECTS})
elseif(SCENARIO STREQUAL "refuses_a_tool_it_cannot_run")
    expect(2 "cannot run|exited 127" "reads nothing of the world" ${GATE} ${RULES} ${FORMAT} ${SILENT}.absent ${SHARED})
else()
    message(FATAL_ERROR "unknown scenario ${SCENARIO}")
endif()
