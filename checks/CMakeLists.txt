add_library(checks_tree STATIC src/tree.c src/languages.c)
target_include_directories(checks_tree PUBLIC src)
conformance_strict_warnings(checks_tree)

foreach(check purity names parity)
    add_executable(weivel-${check} src/${check}.c)
    target_link_libraries(weivel-${check} PRIVATE checks_tree)
    conformance_strict_warnings(weivel-${check})
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
