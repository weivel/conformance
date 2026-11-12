# -DSCENARIO=<name> -DPLATFORM=<linux|macos|windows> -DCURRENT=<the platform this runs on> -DGATE=<exe> -DRULES=<file>
# -DFORMAT=<msvc|gnu|apple> -DTOOL=<dumpbin or nm> -DSILENT=<a tool that prints nothing> -DSHARED=<fixture library>
# -DOBJECTS=<the same fixture as a static library> -DWORK=<a directory for copies>
# -DMISSING_AN_OBJECT=<the fixture without its plant> -DOPAQUE_OBJECTS=<the fixture built with /GL, or none>
# -DCORE_SHARED=<the real core shared library>
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
    foreach(lookup "GetCurrentProcessId[^\n]*\\(process-identity\\)" "GetCurrentThreadId[^\n]*\\(process-identity\\)"
            "IsDebuggerPresent[^\n]*\\(debugger\\)")
        expect(1 "${lookup}; the msvc-dll startup code draws it in, but without the core's objects" ""
            ${the_shared_library})
    endforeach()
elseif(SCENARIO STREQUAL "refuses_a_tool_that_reads_nothing")
    expect(2 "no imports read from [^\n]*so nothing was checked" "reads nothing of the world"
        ${GATE} ${RULES} ${FORMAT} ${SILENT} ${SHARED} ${OBJECTS})
elseif(SCENARIO STREQUAL "refuses_a_tool_it_cannot_run")
    expect(2 "cannot run|exited 127" "reads nothing of the world" ${GATE} ${RULES} ${FORMAT} ${SILENT}.absent ${SHARED})
elseif(SCENARIO STREQUAL "reads_a_library_whose_path_holds_a_space_and_a_quote")
    set(copies "${WORK}/core gate's copy")
    file(REMOVE_RECURSE "${copies}")
    file(COPY "${SHARED}" "${OBJECTS}" DESTINATION "${copies}")
    get_filename_component(shared_name "${SHARED}" NAME)
    get_filename_component(objects_name "${OBJECTS}" NAME)
    expect(0 "the core reads nothing of the world" ""
        ${GATE} ${RULES} ${FORMAT} ${TOOL} "${copies}/${shared_name}" "${copies}/${objects_name}")
elseif(SCENARIO STREQUAL "refuses_objects_the_library_was_not_built_from")
    expect(2 "exports fixture_plant, which 0 of the objects given define: they are not the objects it was built from"
        "reads nothing of the world" ${the_shared_library} ${MISSING_AN_OBJECT})
    expect(2 "exports weivel_[a-z_]+, which 0 of the objects given define" "reads nothing of the world"
        ${GATE} ${RULES} ${FORMAT} ${TOOL} ${CORE_SHARED} ${OBJECTS})
    expect(2 "exports fixture_plant, which 2 of the objects given define" "reads nothing of the world"
        ${the_shared_library_and_its_objects} ${OBJECTS})
elseif(SCENARIO STREQUAL "refuses_an_object_without_a_symbol_table")
    expect(2 "[a-z_]+\\.obj yields no symbol table, so what it calls cannot be judged" "reads nothing of the world"
        ${the_shared_library} ${OPAQUE_OBJECTS})
elseif(SCENARIO STREQUAL "source_scan_refuses_each_pattern")
    set(root "${WORK}/${SCENARIO}")
    file(REMOVE_RECURSE "${root}")
    string(CONCAT parser_word "JS" "ON")
    set(plants
        "fopen|x = fopen(path, mode)" "fopen64|x = fopen64(path, mode)" "fopen_s|fopen_s(&x, path, mode)"
        "freopen|freopen(path, mode, x)" "open|x = open (path, 0)" "CreateFile|CreateFileW(path)"
        "getenv|x = getenv(name)" "secure_getenv|x = secure_getenv(name)"
        "GetEnvironmentVariable|GetEnvironmentVariableA(name, buffer, 1)" "time|x = time(0)" "clock|x = clock ()"
        "clock_gettime|clock_gettime(1, &x)" "timespec_get|timespec_get(&x, 1)" "gettimeofday|gettimeofday(&x, 0)"
        "mach_absolute_time|x = mach_absolute_time()" "QueryPerformanceCounter|QueryPerformanceCounter(&x)"
        "GetTickCount|x = GetTickCount64()" "GetSystemTime|GetSystemTimeAsFileTime(&x)" "__rdtsc|x = __rdtsc()"
        "rdtsc|emit(rdtsc)" "rdtscp|x = rdtscp" "syscall|syscall (39)" "asm|asm(nop)" "__asm|__asm { nop }"
        "json|a ${parser_word} reader")
    set(locks "AcquireSRWLock|AcquireSRWLockExclusive(&x)" "pthread_mutex_|pthread_mutex_lock(&x)")
    set(expected "")
    set(index 0)
    foreach(plant IN LISTS plants locks)
        string(FIND "${plant}" "|" bar)
        string(SUBSTRING "${plant}" 0 ${bar} name)
        math(EXPR content_start "${bar} + 1")
        string(SUBSTRING "${plant}" ${content_start} -1 content)
        file(WRITE "${root}/src/plant_${index}.c" "${content}\n")
        list(APPEND expected "src/plant_${index}\\.c:1: '${name}'")
        math(EXPR index "${index} + 1")
    endforeach()
    execute_process(COMMAND ${GATE} --sources ${RULES} ${root} RESULT_VARIABLE code OUTPUT_VARIABLE out
        ERROR_VARIABLE err)
    if(NOT code EQUAL 1)
        message(FATAL_ERROR "${SCENARIO}: exit ${code}, expected 1\n${out}${err}")
    endif()
    foreach(pattern IN LISTS expected)
        if(NOT "${out}${err}" MATCHES "${pattern}")
            message(FATAL_ERROR "${SCENARIO}: no line matches '${pattern}'\n${out}${err}")
        endif()
    endforeach()
    if(NOT "${out}${err}" MATCHES "'pthread_mutex_' is a lock call outside src/mutex\\.c")
        message(FATAL_ERROR "${SCENARIO}: the lock call is not named as one\n${out}${err}")
    endif()
    message("${out}${err}")
elseif(SCENARIO STREQUAL "source_scan_accepts_lookalikes_and_the_mutex_file")
    set(root "${WORK}/${SCENARIO}")
    file(REMOVE_RECURSE "${root}")
    file(WRITE "${root}/src/lookalike.c"
        "int timeout(int openness) { return runtime_clock + reopen_count + fopened + asmx + datetime(1) + clocks (2) }\n")
    file(WRITE "${root}/src/mutex.c" "AcquireSRWLockExclusive(&lock)\npthread_mutex_lock(&lock)\n")
    file(WRITE "${root}/include/api.h" "int weivel_api(void)\n")
    file(WRITE "${root}/test/plant.c" "x = getenv(name)\n")
    expect(0 "3 sources read.*the core's sources read nothing of the world" "" ${GATE} --sources ${RULES} ${root})
elseif(SCENARIO STREQUAL "source_scan_refuses_an_empty_tree")
    set(root "${WORK}/${SCENARIO}")
    file(REMOVE_RECURSE "${root}")
    file(WRITE "${root}/README.md" "prose\n")
    file(WRITE "${root}/docs/example.c" "x = getenv(name)\n")
    expect(2 "no sources under .*so nothing was checked" "" ${GATE} --sources ${RULES} ${root})
else()
    message(FATAL_ERROR "unknown scenario ${SCENARIO}")
endif()
