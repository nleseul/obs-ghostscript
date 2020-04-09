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
    HINTS ${GSSourcePath}
    PATHS
        /usr/include /usr/local/include /opt/local/include /sw/include)
        
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
set_property (TARGET libobs PROPERTY IMPORTED_LOCATION ${OBS_LIB_PATH})
target_link_libraries (${PROJECT_NAME} libobs)

# Ghostscript
include_directories(${GHOSTSCRIPT_INCLUDE_DIR})
add_library (libgs SHARED IMPORTED)
set_property (TARGET libgs PROPERTY IMPORTED_LOCATION ${GHOSTSCRIPT_LIB_PATH})
target_link_libraries (${PROJECT_NAME} libgs)


if (WIN32)
	install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION obs-plugins/${_lib_suffix}bit)
	install(FILES ${CMAKE_BINARY_DIR}/Debug/${PROJECT_NAME}.pdb DESTINATION obs-plugins/${_lib_suffix}bit CONFIGURATIONS Debug)
	install(FILES ${GHOSTSCRIPT_LIB_DIR}/gsdll${_lib_suffix}.dll DESTINATION obs-plugins/${_lib_suffix}bit)
else()
	install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION obs-plugins/${_lib_suffix}bit)
endif()
install(DIRECTORY data/ DESTINATION data/obs-plugins/${PROJECT_NAME}/)
