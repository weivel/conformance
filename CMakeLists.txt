cmake_minimum_required(VERSION 3.15)
project(weivel_conformance C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

if(NOT EXISTS ${PROJECT_SOURCE_DIR}/siblings/core/CMakeLists.txt)
    message(FATAL_ERROR "siblings/core is missing: link or clone the core checkout there "
        "(for example a directory junction or symlink named siblings/core pointing at the core repository)")
endif()

function(conformance_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

add_subdirectory(siblings/core core)
add_subdirectory(runner)

enable_testing()

function(conformance_test name)
    add_executable(${name} test/${name}.c)
    target_link_libraries(${name} PRIVATE ${ARGN})
    target_include_directories(${name} PRIVATE test)
    conformance_strict_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endfunction()

conformance_test(json_test conformance_json)
