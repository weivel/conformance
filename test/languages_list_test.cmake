# -DLANGUAGES=<file>. The list decides what every role may hold (the `c` lines what the native roles allow, the other
# languages' build files what they refuse), so a kit commit that changes it must change this pin, where it is seen.

set(expected
    "c extension .c"
    "c extension .h"
    "c name CMakeLists.txt"
    "c pattern *.cmake"
    "c source-root src"
    "c test-root test"
    "c header-root include/weivel/c"
    "java extension .java"
    "java name build.gradle"
    "java name settings.gradle"
    "java name gradle.properties"
    "java name gradlew"
    "java name gradlew.bat"
    "java prefix gradle/wrapper/"
    "java source-root src/main/java"
    "java test-root src/test/java"
    "typescript extension .ts"
    "typescript name package.json"
    "typescript name package-lock.json"
    "typescript pattern tsconfig*.json"
    "typescript source-root src"
    "typescript test-root test")

file(STRINGS "${LANGUAGES}" lines)
set(found "")
foreach(line IN LISTS lines)
    string(STRIP "${line}" trimmed)
    if(NOT trimmed STREQUAL "")
        list(APPEND found "${trimmed}")
    endif()
endforeach()

foreach(line IN LISTS found)
    list(FIND expected "${line}" at)
    if(at EQUAL -1)
        message(SEND_ERROR "${LANGUAGES} holds the unpinned line '${line}'")
    endif()
endforeach()
foreach(line IN LISTS expected)
    list(FIND found "${line}" at)
    if(at EQUAL -1)
        message(SEND_ERROR "${LANGUAGES} lacks the pinned line '${line}'")
    endif()
endforeach()
if(NOT found STREQUAL expected)
    message(FATAL_ERROR "${LANGUAGES} does not hold exactly the pinned lines, in their order")
endif()
list(LENGTH found count)
message(STATUS "${LANGUAGES} holds exactly the ${count} pinned lines")
