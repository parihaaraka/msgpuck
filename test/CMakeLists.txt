cmake_minimum_required(VERSION 4.0)
#if(POLICY CMP0037)
#    cmake_policy(SET CMP0037 OLD) # don't blame custom target names
#endif(POLICY CMP0037)

include_directories("../")

#find_program(PROVE prove)
if (PROVE)
    set(TEST_RUNNER prove)
else()
    set(TEST_RUNNER)
endif()

file(GLOB LIB_FILES ../*.c)

set(alltests)
foreach (test msgpuck)
    add_executable(${test}.test ${test}.cc test.c ${LIB_FILES})
    #target_link_libraries(${test}.test msgpuck)

    list(APPEND alltests ${test}.test_run)
    add_custom_target(${test}.test_run
        DEPENDS ${test}.test
        COMMAND ${TEST_RUNNER} ${PROJECT_BINARY_DIR}/test/${test}.test)
endforeach()

add_custom_target(test DEPENDS ${alltests})
