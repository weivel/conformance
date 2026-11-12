add_library(checks_tree STATIC src/tree.c src/languages.c src/tool.c)
target_include_directories(checks_tree PUBLIC src)
conformance_strict_warnings(checks_tree)

foreach(check purity names parity core_gate)
    string(REPLACE "_" "-" program ${check})
    add_executable(weivel-${program} src/${check}.c)
    target_link_libraries(weivel-${program} PRIVATE checks_tree)
    conformance_strict_warnings(weivel-${program})
endforeach()

set(checks_test_scenarios
    purity_rejects_a_typescript_file_in_a_c_repo
    purity_rejects_a_c_file_in_a_java_repo
    purity_rejects_a_generated_js_file_in_a_java_repo
    purity_accepts_a_clean_c_repo
    purity_accepts_a_clean_java_repo
    purity_accepts_a_clean_typescript_repo_and_rejects_a_stray_script
    purity_skips_build_output
    purity_refuses_an_empty_directory
    purity_rejects_editor_and_cache_directories
    purity_rejects_language_build_files_in_a_c_repo
    purity_refuses_an_unknown_role
    names_rejects_a_header_and_prose_but_not_tool_state
    names_accepts_a_clean_tree
    names_skips_build_output_binary_files_and_the_words_file
    names_reads_text_beside_a_binary_file
    names_prints_each_skipped_binary_file
    names_reads_a_text_file_whose_first_nul_comes_late
    names_refuses_an_empty_directory
    names_refuses_an_empty_word_list
    parity_rejects_a_missing_module
    parity_rejects_a_module_not_on_the_list
    parity_accepts_every_module_with_or_without_the_optional_one
    parity_finds_java_modules_under_its_source_root
    parity_rejects_an_empty_module_directory
    parity_refuses_an_empty_directory
    parity_refuses_an_empty_module_list)

foreach(scenario IN LISTS checks_test_scenarios)
    add_test(NAME ${scenario}
        COMMAND ${CMAKE_COMMAND} -DSCENARIO=${scenario} -DWORK=${PROJECT_BINARY_DIR}/checks-work
            -DPURITY=$<TARGET_FILE:weivel-purity> -DNAMES=$<TARGET_FILE:weivel-names>
            -DPARITY=$<TARGET_FILE:weivel-parity>
            -DLANGUAGES=${CMAKE_CURRENT_SOURCE_DIR}/languages.txt -DWORDS=${CMAKE_CURRENT_SOURCE_DIR}/names.txt
            -DMODULES=${PROJECT_SOURCE_DIR}/siblings/core/MODULES
            -DOPAQUE=${PROJECT_SOURCE_DIR}/test/fixtures/opaque-bytes
            -P ${PROJECT_SOURCE_DIR}/test/checks_test.cmake)
endforeach()

add_test(NAME kit_is_pure
    COMMAND weivel-purity ${PROJECT_SOURCE_DIR} kit ${CMAKE_CURRENT_SOURCE_DIR}/languages.txt)
add_test(NAME kit_names_no_foreign_word
    COMMAND weivel-names ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/names.txt)
add_test(NAME core_is_pure
    COMMAND weivel-purity ${PROJECT_SOURCE_DIR}/siblings/core core ${CMAKE_CURRENT_SOURCE_DIR}/languages.txt)

set(core_words ${CMAKE_CURRENT_SOURCE_DIR}/names.txt)
if(EXISTS ${PROJECT_SOURCE_DIR}/siblings/core/test/names.txt)
    set(core_words ${PROJECT_SOURCE_DIR}/siblings/core/test/names.txt)
endif()
add_test(NAME core_names_no_foreign_word
    COMMAND weivel-names ${PROJECT_SOURCE_DIR}/siblings/core ${core_words})

if(MSVC)
    set(core_gate_platform windows)
    set(core_gate_format msvc)
    get_filename_component(compiler_directory "${CMAKE_C_COMPILER}" DIRECTORY)
    get_filename_component(linker_directory "${CMAKE_LINKER}" DIRECTORY)
    find_program(CORE_GATE_DUMPBIN dumpbin HINTS "${compiler_directory}" "${linker_directory}" NO_DEFAULT_PATH)
    if(NOT CORE_GATE_DUMPBIN)
        message(FATAL_ERROR "dumpbin was found neither beside the compiler (${compiler_directory}) nor beside the "
            "linker (${linker_directory}), so the core gate cannot read a DLL's import table")
    endif()
    set(core_gate_tool "${CORE_GATE_DUMPBIN}")
else()
    if(APPLE)
        set(core_gate_platform macos)
        set(core_gate_format apple)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(core_gate_platform linux)
        set(core_gate_format gnu)
    else()
        message(FATAL_ERROR "the core gate knows no import reader for ${CMAKE_SYSTEM_NAME} with ${CMAKE_C_COMPILER_ID}")
    endif()
    if(NOT CMAKE_NM)
        message(FATAL_ERROR "no nm found, so the core gate cannot read the core's imports")
    endif()
    set(core_gate_tool "${CMAKE_NM}")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
endif()

set(core_gate_rules ${CMAKE_CURRENT_SOURCE_DIR}/core_gate.txt)
set(core_gate_fixtures ${PROJECT_SOURCE_DIR}/test/fixtures/core-gate)

add_library(core_gate_parser_provider SHARED ${core_gate_fixtures}/parser_provider.c)
conformance_strict_warnings(core_gate_parser_provider)

add_executable(core_gate_silent_tool ${core_gate_fixtures}/silent_tool.c)
conformance_strict_warnings(core_gate_silent_tool)

function(core_gate_fixture plant)
    foreach(kind SHARED STATIC)
        string(TOLOWER ${kind} suffix)
        set(target core_gate_${plant}_${suffix})
        add_library(${target} ${kind}
            ${core_gate_fixtures}/base.c ${core_gate_fixtures}/mutex.c ${core_gate_fixtures}/plant_${plant}.c)
        conformance_strict_warnings(${target})
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
        if(NOT WIN32)
            target_link_libraries(${target} PRIVATE Threads::Threads)
        endif()
        if(plant STREQUAL "parser_import")
            target_link_libraries(${target} PRIVATE core_gate_parser_provider)
        endif()
    endforeach()
endfunction()

set(core_gate_scenarios
    accepts_a_clean_fixture:nothing
    refuses_getenv:getenv
    refuses_fopen:fopen
    refuses_time:time
    refuses_a_parser_it_imports:parser_import
    refuses_a_parser_inside_it:parser_inside
    refuses_a_lock_call_outside_the_mutex:lock_elsewhere
    says_where_lock_calls_cannot_be_attributed:lock_elsewhere
    clears_a_startup_clock_only_through_the_objects:nothing
    refuses_a_tool_that_reads_nothing:nothing
    refuses_a_tool_it_cannot_run:nothing)
set(core_gate_scenarios_on_windows_only clears_a_startup_clock_only_through_the_objects)
set(core_gate_scenarios_alike_everywhere refuses_a_tool_that_reads_nothing refuses_a_tool_it_cannot_run)

set(core_gate_plants "")
foreach(entry IN LISTS core_gate_scenarios)
    string(REPLACE ":" ";" parts ${entry})
    list(GET parts 1 plant)
    list(APPEND core_gate_plants ${plant})
endforeach()
list(REMOVE_DUPLICATES core_gate_plants)
foreach(plant IN LISTS core_gate_plants)
    core_gate_fixture(${plant})
endforeach()

foreach(entry IN LISTS core_gate_scenarios)
    string(REPLACE ":" ";" parts ${entry})
    list(GET parts 0 scenario)
    list(GET parts 1 plant)
    set(platforms linux macos windows)
    if(scenario IN_LIST core_gate_scenarios_on_windows_only)
        set(platforms windows)
    elseif(scenario IN_LIST core_gate_scenarios_alike_everywhere)
        set(platforms ${core_gate_platform})
    endif()
    foreach(platform IN LISTS platforms)
        set(name core_gate_${scenario}_on_${platform})
        if(scenario IN_LIST core_gate_scenarios_alike_everywhere)
            set(name core_gate_${scenario})
        endif()
        add_test(NAME ${name}
            COMMAND ${CMAKE_COMMAND} -DSCENARIO=${scenario} -DPLATFORM=${platform} -DCURRENT=${core_gate_platform}
                -DGATE=$<TARGET_FILE:weivel-core-gate> -DRULES=${core_gate_rules} -DFORMAT=${core_gate_format}
                -DTOOL=${core_gate_tool} -DSILENT=$<TARGET_FILE:core_gate_silent_tool>
                -DSHARED=$<TARGET_FILE:core_gate_${plant}_shared> -DOBJECTS=$<TARGET_FILE:core_gate_${plant}_static>
                -P ${PROJECT_SOURCE_DIR}/test/core_gate_test.cmake)
        set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77)
    endforeach()
endforeach()

add_test(NAME core_reads_nothing_of_the_world
    COMMAND weivel-core-gate ${core_gate_rules} ${core_gate_format} ${core_gate_tool}
        $<TARGET_FILE:weivel_core_shared> $<TARGET_FILE:weivel_core>)
