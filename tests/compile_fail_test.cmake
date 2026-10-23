# A wrapper whose shape is not its map entry's does not compile. Each case of
# tests/compile_fail/wrappers.cpp (the targets compile_fail_<n>) must fail with its own static_assert,
# and case 0, the same wrappers written right, must build, so a failure is the assert's and never a
# broken build. Run by ctest with -DBUILD=<build dir>, in the compiler environment of the configure.
set(expect_1 "the wrapper's arity is not its entry's")  # a method one argument short
set(expect_2 "the wrapper's arity is not its entry's")  # an icall with one too many
set(expect_3 "the wrapper's arity is not its entry's")  # a static method given an object
set(expect_4 "T's size is not the size of the entry's type")  # a field
set(expect_5 "T's size is not the size of the entry's type")  # a static field
set(expect_6 "EnumValue wraps a map entry of kind EnumValue")
set(expect_7 "Type wraps a map entry of kind Type")

execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BUILD}" --target compile_fail_0
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "the wrappers written right do not compile\n${out}")
endif()

foreach(case RANGE 1 7)
  execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BUILD}" --target compile_fail_${case}
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE out)
  if(code EQUAL 0)
    message(FATAL_ERROR "case ${case} compiled; it must fail with \"${expect_${case}}\"")
  endif()
  string(FIND "${out}" "${expect_${case}}" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "case ${case} failed without \"${expect_${case}}\"\n${out}")
  endif()
  message(STATUS "case ${case} does not compile: ${expect_${case}}")
endforeach()
