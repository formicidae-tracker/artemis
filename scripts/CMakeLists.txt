find_program(GO_EXECUTABLE go REQUIRED)

set(RESOURCIFY_EXECUTABLE ${PROJECT_BINARY_DIR}/scripts/resourcify/resourcify)
set(RESOURCIFY_EXECUTABLE ${PROJECT_BINARY_DIR}/scripts/resourcify/resourcify PARENT_SCOPE)

add_custom_command(OUTPUT ${RESOURCIFY_EXECUTABLE}
                   COMMAND ${GO_EXECUTABLE} ARGS "build" "-o" ${RESOURCIFY_EXECUTABLE}
                   WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/scripts/resourcify
                   DEPENDS ${PROJECT_SOURCE_DIR}/scripts/resourcify/main.go)

add_custom_target(resourcify
                  DEPENDS ${RESOURCIFY_EXECUTABLE})
