# A wrapper whose shape is not its map entry's does not compile, nor does a typed handle used as
# another type. Each case of tests/compile_fail/wrappers.cpp (the targets compile_fail_<n>) must fail
# with its own error, a static_assert or a conversion the handles do not give, and case 0, the same
# code written right, must build, so a failure is the case's and never a broken build. Run by ctest
# with -DBUILD=<build dir>, in the compiler environment of the configure.
set(expect_1 "the wrapper's arity is not its entry's")  # a method one argument short
set(expect_2 "the wrapper's arity is not its entry's")  # an icall with one too many
set(expect_3 "the wrapper's arity is not its entry's")  # a static method given an object
set(expect_4 "T's size is not the size of the entry's type")  # a field
set(expect_5 "T's size is not the size of the entry's type")  # a static field
set(expect_6 "EnumValue wraps a map entry of kind EnumValue")
set(expect_7 "Type wraps a map entry of kind Type")
# An unrelated handle, a base handle taken as a derived one, and a raw reference taken as a handle: MSVC's
# error codes for an argument and a value it cannot convert, which its wording may change around.
set(expect_8 "error C2664")
set(expect_9 "error C2440")
set(expect_10 "error C2440")
# A handle wrapper's argument of an unrelated handle, through the typed call (11) and through the
# transitional raw bridge (12): no overload takes it, which MSVC reports for a call on the wrapper
# object as no matching call operator. A Ref built straight from a void* (13), and a field of a
# pointer type (14).
set(expect_11 "error C3889")
set(expect_12 "error C3889")
set(expect_13 "error C2440")
set(expect_14 "a managed reference field is Field<D, void*>")
# A hook's detour naming a handle of the wrong type: the raw overload's constraint rejects it (it
# takes exactly RawDetour) and the typed overload cannot convert it, which MSVC reports as cannot
# convert argument.
set(expect_15 "error C2664")
# A HeldRef of an unrelated type for a handle parameter (16), and a Ref of an unrelated type stored
# into a handle field (17): the bridge's exact-name rule takes only void*, const void*, nullptr and
# a plain Held for a handle, and no conversion gives the typed parameter. Measured codes.
set(expect_16 "error C3889")
set(expect_17 "error C2672")
# A Ref of an unrelated type at a handle parameter, where the bridge now takes exactly an Object:
# the exact-name rule still refuses it. Measured code.
set(expect_18 "error C3889")

execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BUILD}" --target compile_fail_0
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "the wrappers written right do not compile\n${out}")
endif()

foreach(case RANGE 1 18)
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
