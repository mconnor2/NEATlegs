# End-to-end determinism: two hopper runs with the same seed must produce
# identical genomes and species statistics, even though fitness is
# evaluated on many threads.  (stats.csv is skipped: it holds timings.)
# Re-evaluating the saved best genome (hopper -e) must give its recorded
# fitness exactly.
#
# cmake -DHOPPER=path -DCONFIG=file.cfg -DOUT=dir [-DGENS=n] -P determinism.cmake

if(NOT GENS)
    set(GENS 12)
endif()

file(REMOVE_RECURSE "${OUT}")
foreach(run a b)
    execute_process(
        COMMAND "${HOPPER}" -C "${CONFIG}" -N ${GENS} -S 12345 -o "${OUT}/${run}"
        RESULT_VARIABLE result
        OUTPUT_QUIET)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "hopper run ${run} failed: ${result}")
    endif()
endforeach()

file(GLOB_RECURSE files RELATIVE "${OUT}/a" "${OUT}/a/*")
list(REMOVE_ITEM files stats.csv)
list(LENGTH files nFiles)
if(nFiles LESS 4)
    message(FATAL_ERROR "expected run output, found: ${files}")
endif()

foreach(f ${files})
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files "${OUT}/a/${f}" "${OUT}/b/${f}"
        RESULT_VARIABLE differ)
    if(differ)
        message(FATAL_ERROR "same seed, different result: ${f}")
    endif()
endforeach()
message(STATUS "${nFiles} files identical across two runs")

file(STRINGS "${OUT}/a/best.genome" header REGEX "^# fitness ")
string(REPLACE "# fitness " "" recorded "${header}")
execute_process(
    COMMAND "${HOPPER}" -C "${CONFIG}" -e "${OUT}/a/best.genome"
    OUTPUT_VARIABLE evaluated
    RESULT_VARIABLE result)
if(NOT result EQUAL 0 OR NOT evaluated MATCHES "^fitness ([^ ]+) ")
    message(FATAL_ERROR "hopper -e failed: ${result} ${evaluated}")
endif()
if(NOT CMAKE_MATCH_1 STREQUAL recorded)
    message(FATAL_ERROR "best.genome recorded fitness ${recorded}, "
                        "re-evaluated ${CMAKE_MATCH_1}")
endif()
message(STATUS "best genome re-evaluates to its fitness ${recorded}")
