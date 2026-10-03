# Each test is a small standalone executable returning 0 on success.
# Tests that need ROMs/media get the repository root as SAPIEMU_SOURCE_DIR (via sapicore).

function(sapi_test name)
    add_executable(${name} ${name}.cpp test_util.h)
    target_link_libraries(${name} PRIVATE sapicore)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

sapi_test(test_z80_smoke)
