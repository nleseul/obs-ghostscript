cmake_minimum_required (VERSION 2.8.12)

foreach(flag_var
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
   if(${flag_var} MATCHES "/MD")
      string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
   endif(${flag_var} MATCHES "/MD")
endforeach(flag_var)

project (obs-ghostscript)

include(CheckCSourceCompiles)

if (OBSSourcePath OR DEFINED ENV{OBSSourcePath})
    # Set already
else()
    set(OBSSourcePath "" CACHE PATH "Path to OBS source code (e.g., C:/Dev/obs-studio/libobs/)")
    message("OBSSourcePath is missing. Please set this variable to the location of the OBS source (e.g., C:/Dev/obs-studio/libobs/).")
endif()

if (NOT OBSLibaryPath AND NOT DEFINED ENV{OBSLibraryPath})
    set (OBSLibraryPath "" CACHE PATH "Path to OBS libraries (e.g., C:/Dev/obs-studio/build64/libobs/Release/)")
endif()

if (NOT GSSourcePath AND NOT DEFINED ENV{GSSourcePath})
    set (GSSourcePath "" CACHE PATH "Path to Ghostscript source code (e.g., C:/Dev/gnu-ghostscript-9.21/)")
endif()

if (NOT GSLibraryPath AND NOT DEFINED ENV{GSLibraryPath})
    set (GSLibraryPath "" CACHE PATH "Path to Ghostscript libraries (e.g., C:/Program Files/gs/gs9.21/bin)")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_lib_suffix 64)
else()
	set(_lib_suffix 32)
endif()

find_library(OBS_LIB_PATH
    NAMES obs
    HINTS
        ${OBSLibraryPath})

find_path(GHOSTSCRIPT_INCLUDE_DIR
    NAMES ghostscript/iapi.h ghostscript/gdevdsp.h
    HINTS ${GSSourcePath})

find_path(GHOSTSCRIPT_SOURCE_DIR
        NAMES psi/iapi.h devices/gdevdsp.h
        HINTS ${GSSourcePath})

if (GHOSTSCRIPT_SOURCE_DIR AND NOT GHOSTSCRIPT_INCLUDE_DIR)
    set(GHOSTSCRIPT_INCLUDE_DIR ${GHOSTSCRIPT_SOURCE_DIR})
    set(GHOSTSCRIPT_IN_SOURCE_TREE 1)
else()
    set(GHOSTSCRIPT_IN_SOURCE_TREE 0)
endif()
        
find_library(GHOSTSCRIPT_LIB_PATH
    NAMES libgs.so gsdll${_lib_suffix}.lib
    HINTS
        ${GSLibraryPath})
        
if (NOT GHOSTSCRIPT_INCLUDE_DIR OR NOT GHOSTSCRIPT_LIB_PATH)
    message("Ghostscript headers or libraries could not be found! Please ensure that Ghostscript is installed somewhere, and set the GSSourcePath and GSLibraryPath variables if necessary.")
endif()

configure_file(${CMAKE_SOURCE_DIR}/src/config.h.in ${CMAKE_BINARY_DIR}/config/config.h)
	
# Source
file (GLOB SOURCES ${CMAKE_SOURCE_DIR}/src/*.c)
file (GLOB HEADER_FILES ${CMAKE_SOURCE_DIR}/include/*.h ${CMAKE_BINARY_DIR}/config/*.h)

include_directories (include ${CMAKE_BINARY_DIR}/config)
add_library (${PROJECT_NAME} SHARED
	${SOURCES}
	${HEADER_FILES}
)

# libobs
include_directories(${OBSSourcePath})
add_library (libobs SHARED IMPORTED)
if (WIN32)
    get_filename_component(_obs_lib_dir ${OBS_LIB_PATH} DIRECTORY)

    set_property (TARGET libobs PROPERTY IMPORTED_IMPLIB ${OBS_LIB_PATH})
    set_property (TARGET libobs PROPERTY IMPORTED_LOCATION ${_obs_lib_dir}/obs.dll)
else()
    set_property (TARGET libobs PROPERTY IMPORTED_LOCATION ${OBS_LIB_PATH})
endif()
target_link_libraries (${PROJECT_NAME} libobs)

# Ghostscript
include_directories(${GHOSTSCRIPT_INCLUDE_DIR})
add_library (libgs SHARED IMPORTED)
if (WIN32)
    get_filename_component(_gs_lib_dir ${GHOSTSCRIPT_LIB_PATH} DIRECTORY)

    set_property (TARGET libgs PROPERTY IMPORTED_IMPLIB ${GHOSTSCRIPT_LIB_PATH})
    set_property (TARGET libgs PROPERTY IMPORTED_LOCATION ${_gs_lib_dir}/gsdll${_lib_suffix}.dll)
else()
    set_property (TARGET libgs PROPERTY IMPORTED_LOCATION ${GHOSTSCRIPT_LIB_PATH})
endif()
target_link_libraries (${PROJECT_NAME} libgs)

if (GHOSTSCRIPT_IN_SOURCE_TREE EQUAL 1)
    add_definitions(/DGS_IN_SOURCE_TREE)
endif()


if (WIN32)
	install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION obs-plugins/${_lib_suffix}bit)
	install(FILES ${CMAKE_BINARY_DIR}/Debug/${PROJECT_NAME}.pdb DESTINATION obs-plugins/${_lib_suffix}bit CONFIGURATIONS Debug)
	install(FILES ${_gs_lib_dir}/gsdll${_lib_suffix}.dll DESTINATION obs-plugins/${_lib_suffix}bit)
else()
	install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION obs-plugins/${_lib_suffix}bit)
endif()
install(DIRECTORY data/ DESTINATION data/obs-plugins/${PROJECT_NAME}/)
