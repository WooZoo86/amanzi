# -*- mode: cmake -*-
include(TestManager)
include(LibraryManager)

include(PreprocessBoxLibFortran)

# Set locations for amr-related source files, make available outside this dir afterward
set(PMAMR_REL_INCLUDE_DIRS problem_setup
                           simulation_driver
                           utils
                           materials
                           region
                           mpc_pk/transport/diffusion
                           mpc_pk/transport/advection
                           mpc_pk/flow
                           mpc_pk
                           mpc_pk/chemistry)
set(PMAMR_INCLUDE_DIRS)
foreach(d ${PMAMR_REL_INCLUDE_DIRS})
  list(APPEND PMAMR_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/${d})
endforeach(d)

foreach(d ${PMAMR_REL_INCLUDE_DIRS})
  add_subdirectory(${d})
endforeach(d)

add_amanzi_library(structured_amanzi
                   SOURCE $<TARGET_OBJECTS:samr>
                              $<TARGET_OBJECTS:sprob>
                              $<TARGET_OBJECTS:sdriver>
                              $<TARGET_OBJECTS:sutils>
                              $<TARGET_OBJECTS:smaterials>
                              $<TARGET_OBJECTS:sregion>
                              $<TARGET_OBJECTS:strans>
                              $<TARGET_OBJECTS:sadv>
                              $<TARGET_OBJECTS:sflow>
                              $<TARGET_OBJECTS:schem>)


set_target_properties(structured_amanzi PROPERTIES LINKER_LANGUAGE C)
if (APPLE AND BUILD_SHARED_LIBS)
  set_target_properties(structured_amanzi PROPERTIES LINK_FLAGS "-Wl,-undefined,dynamic_lookup")
endif()			  
