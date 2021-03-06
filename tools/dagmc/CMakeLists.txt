# Find CGM directory
find_package( CGM )

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_BINARY_DIR}/src )

if ( CGM_FOUND )
  include_directories( ${CGM_INCLUDE_DIRS} )
endif( )

add_library(dagmc DagMC.hpp DagMC.cpp)
target_link_libraries(dagmc  MOAB)
set_target_properties(
  dagmc
  PROPERTIES
  VERSION ${MOAB_VERSION}
  SOVERSION ${MOAB_VERSION_MAJOR}
  )
install(
  TARGETS dagmc
  EXPORT MOABTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
set( MOAB_INSTALL_TARGETS ${MOAB_INSTALL_TARGETS} dagmc)
install(
  FILES DagMC.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

add_executable(
  dagmc_preproc
  dagmc_preproc.cpp
  obb_analysis.cpp
  dagmc_preproc.hpp
  )
target_link_libraries( dagmc_preproc MOAB dagmc ${CGM_LIBRARIES} )
set( MOAB_HAVE_DAGMC 1 )
install(
  TARGETS dagmc_preproc
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

if ( MOAB_ENABLE_TESTING )
  add_executable( test_geom test_geom.cc )
  set_target_properties( test_geom PROPERTIES COMPILE_FLAGS "${MOAB_DEFINES} -DTEST" )
  target_link_libraries( test_geom dagmc MOAB ${CGM_LIBRARIES})
  add_test( test_geom ${EXECUTABLE_OUTPUT_PATH}/test_geom )
endif()

if ( MOAB_HAVE_CGM )
  target_link_libraries( dagmc ${CGM_LIBRARIES} )
  if ( MOAB_ENABLE_TESTING )
    target_link_libraries( test_geom ${CGM_LIBRARIES} )
  endif()
  option ( MOAB_BUILD_CGM2MOAB    "Build cgm2moab?"  OFF )
  if(MOAB_BUILD_CGM2MOAB)
    add_executable( cgm2moab dagmc_preproc.cpp
                             obb_analysis.cpp
                             dagmc_preproc.hpp )
    target_link_libraries( cgm2moab
                           dagmc
                           MOAB
                           ${CGM_LIBRARIES} )
    install(
      TARGETS cgm2moab
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      )
  endif()
endif( )
