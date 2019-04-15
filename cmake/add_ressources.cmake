function(ADD_RESOURCES out_var)
  set(result)
  foreach(in_f ${ARGN})
	  file(RELATIVE_PATH src_f ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${in_f})
	  set(out_f "${PROJECT_BINARY_DIR}/${src_f}.o")
	  add_custom_command(OUTPUT ${out_f}
		                 COMMAND ld -r -b binary -o ${out_f} ${src_f}
		                 DEPENDS ${in_f}
		                 WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
		                 COMMENT "Building Ressource object ${out_f}"
		                 VERBATIM
		                 )
	  list(APPEND result ${out_f})
  endforeach()
  set(${out_var} "${result}" PARENT_SCOPE)
endfunction()
