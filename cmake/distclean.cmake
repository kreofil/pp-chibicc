if(NOT DEFINED SOURCE_DIR OR NOT DEFINED BINARY_DIR)
  message(FATAL_ERROR "SOURCE_DIR and BINARY_DIR are required")
endif()

if(SOURCE_DIR STREQUAL BINARY_DIR)
  message(FATAL_ERROR "Refusing to distclean an in-source build")
endif()

file(REMOVE_RECURSE "${SOURCE_DIR}/bin")
file(REMOVE_RECURSE "${BINARY_DIR}")
message(STATUS "Removed ${BINARY_DIR} and ${SOURCE_DIR}/bin")
