#  -*- mode: cmake -*-

#
# Build TPL:  ParMetis 
#
# --- Define all the directories and common external project flags
define_external_project_args(ParMetis
                             TARGET parmetis
                             DEPENDS METIS)

# add version version to the autogenerated tpl_versions.h file
amanzi_tpl_version_write(FILENAME ${TPL_VERSIONS_INCLUDE_FILE}
  PREFIX ParMetis
  VERSION ${ParMetis_VERSION_MAJOR} ${ParMetis_VERSION_MINOR} ${ParMetis_VERSION_PATCH})

set(ParMetis_GKLIB_DIR ${METIS_source_dir}/GKlib)
set(ParMetis_METIS_DIR ${METIS_source_dir})
# --- Define the CMake configure parameters
# Note:
#      CMAKE_CACHE_ARGS requires -DVAR:<TYPE>=VALUE syntax
#      CMAKE_ARGS -DVAR=VALUE OK
# NO WHITESPACE between -D and VAR. Parser blows up otherwise.
set(ParMetis_CMAKE_CACHE_ARGS
                  -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
                  -DCMAKE_INSTALL_PREFIX:STRING=<INSTALL_DIR>
                  -DSHARED:BOOL=${BUILD_SHARED_LIBS}
                  -DGKLIB_PATH:PATH=${ParMetis_GKLIB_DIR}
                  -DMETIS_PATH:PATH=${ParMetis_METIS_DIR})

# --- Add external project build and tie to the ParMetis build target
ExternalProject_Add(${ParMetis_BUILD_TARGET}
                    DEPENDS   ${ParMetis_PACKAGE_DEPENDS}             # Package dependency target
                    TMP_DIR   ${ParMetis_tmp_dir}                     # Temporary files directory
                    STAMP_DIR ${ParMetis_stamp_dir}                   # Timestamp and log directory
                    # -- Download and URL definitions
                    DOWNLOAD_DIR ${TPL_DOWNLOAD_DIR} 
                    URL          ${ParMetis_URL}                      # URL may be a web site OR a local file
                    URL_MD5      ${ParMetis_MD5_SUM}                  # md5sum of the archive file
                    # -- Configure
                    SOURCE_DIR       ${ParMetis_source_dir}           # Source directory
                    CMAKE_CACHE_ARGS ${AMANZI_CMAKE_CACHE_ARGS}       # Global definitions from root CMakeList
                                     ${ParMetis_CMAKE_CACHE_ARGS}     
                                     -DCMAKE_C_FLAGS:STRING=${Amanzi_COMMON_CFLAGS}  # Ensure uniform build
                                     -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
                    # -- Build
                    BINARY_DIR        ${ParMetis_build_dir}           # Build directory 
                    BUILD_COMMAND     $(MAKE)                     # $(MAKE) enables parallel builds through make
                    BUILD_IN_SOURCE   ${ParMetis_BUILD_IN_SOURCE}     # Flag for in source builds
                    # -- Install
                    INSTALL_DIR      ${TPL_INSTALL_PREFIX}        # Install directory
                    # -- Output control
                    ${ParMetis_logging_args})

# --- Useful variables that depend on ZlIB (HDF5, NetCDF)
include(BuildLibraryName)
build_library_name(z ParMetis_LIBRARIES APPEND_PATH ${TPL_INSTALL_PREFIX}/lib)
set(ParMetis_INCLUDE_DIRS ${TPL_INSTALL_PREFIX}/include)
