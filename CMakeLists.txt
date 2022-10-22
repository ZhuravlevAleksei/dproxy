cmake_minimum_required(VERSION 3.0)
project(DP VERSION 0.0.0.1 DESCRIPTION "DNS proxy" LANGUAGES C)

include_directories(
    ${DP_SOURCE_DIR}/libyaml/include
    ${DP_SOURCE_DIR}/dproxy/src
    ${DP_SOURCE_DIR}/hiredis
    ${DP_SOURCE_DIR}/cJSON
    ${DP_SOURCE_DIR}/oniguruma
    ${DP_SOURCE_DIR}/oniguruma/src)

add_subdirectory(libyaml)
add_subdirectory(hiredis)
add_subdirectory(cJSON)
add_subdirectory(oniguruma)

set(SOURCES)

# SRC FOLDER
set(SRC_PATH ./src/)

set(SRC_FILES 
    dp.c
    conf.c
    storage.c
    server.c
    package.c
    filter.c
    client.c
)

foreach(FILE ${SRC_FILES})
    set(SOURCES "${SOURCES}" "${SRC_PATH}${FILE}")
endforeach()


add_executable(dp ${SOURCES})
target_link_libraries(dp PUBLIC yaml hiredis resolv cjson onig pthread)

