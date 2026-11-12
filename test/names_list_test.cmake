# -DWORDS=<file>. A kit's drift judges it by its own list, so a kit commit must not shrink it; words are split in
# pieces because the names gate reads this file too.

string(CONCAT expected
    "spi" "sor;"
    "fore" "bay;"
    "cla" "ude;"
    "open" "code;"
    "anth" "ropic;"
    "int" "isy;"
    "git" "hub;"
    "bayo" "net;"
    "base" "kit")

file(STRINGS "${WORDS}" lines)
set(words "")
foreach(line IN LISTS lines)
    string(STRIP "${line}" word)
    if(NOT word STREQUAL "")
        list(APPEND words "${word}")
    endif()
endforeach()

list(LENGTH words count)
if(NOT count EQUAL 9)
    message(FATAL_ERROR "${WORDS} holds ${count} words, expected 9")
endif()
if(NOT words STREQUAL expected)
    message(FATAL_ERROR "${WORDS} holds '${words}', expected '${expected}'")
endif()
message(STATUS "${WORDS} holds exactly the 9 pinned words")
