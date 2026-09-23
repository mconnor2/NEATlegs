# End-to-end determinism: two hopper runs with the same seed must produce
# identical genomes and species statistics, even though fitness is
# evaluated on many threads.  (stats.csv is skipped: it holds timings.)
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
