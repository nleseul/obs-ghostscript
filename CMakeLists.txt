cmake_minimum_required (VERSION 2.8.12)
project (obs-ghostscript)

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

find_path(OBS_LIB_DIR
    NAMES obs.dll obs.lib
    HINTS
        ${OBSSourcePath}/../build/libobs/Release/
        ${OBSSourcePath}/../build${_lib_suffix}/libobs/Release/
        ${OBSSourcePath}/../build/libobs/Debug/
        ${OBSSourcePath}/../build${_lib_suffix}/libobs/Debug/
    PATHS
		/usr/lib /usr/local/lib /opt/local/lib /sw/lib)
        
find_path(GHOSTSCRIPT_INCLUDE_DIR
    NAMES psi/iapi.h devices/gdevdsp.h
    HINTS ${GSSourcePath}
    PATHS
        /usr/include /usr/local/include /opt/local/include /sw/include)
        
find_path(GHOSTSCRIPT_LIB_DIR
    NAMES gsdll${_lib_suffix}.dll gsdll${_lib_suffix}.lib
    HINTS
        ${GSLibraryPath}
    PATHS
        /usr/lib /usr/local/lib /opt/local/lib /sw/lib)
        
if (NOT GHOSTSCRIPT_INCLUDE_DIR OR NOT GHOSTSCRIPT_LIB_DIR)
    message("Ghostscript headers or libraries could not be found! Please ensure that Ghostscript is installed somewhere, and set the GSSourcePath and GSLibraryPath variables if necessary.")
endif()
	
# Source
file (GLOB SOURCES ${CMAKE_SOURCE_DIR}/src/*.c)
file (GLOB HEADER_FILES ${CMAKE_SOURCE_DIR}/include/*.h)

include_directories (include)
add_library (${PROJECT_NAME} SHARED
	${SOURCES}
	${HEADER_FILES}
)

# libobs
include_directories(${OBSSourcePath})
add_library (libobs SHARED IMPORTED)
set_property (TARGET libobs PROPERTY IMPORTED_LOCATION ${OBS_LIB_DIR}/obs.dll)
set_property (TARGET libobs PROPERTY IMPORTED_IMPLIB ${OBS_LIB_DIR}/obs.lib)
target_link_libraries (${PROJECT_NAME} libobs)

# Ghostscript
include_directories(${GHOSTSCRIPT_INCLUDE_DIR})
add_library (gsdll SHARED IMPORTED)
set_property (TARGET gsdll PROPERTY IMPORTED_LOCATION ${OBS_LIB_DIR}/gsdll${_lib_suffix}.dll)
set_property (TARGET gsdll PROPERTY IMPORTED_IMPLIB ${GHOSTSCRIPT_LIB_DIR}/gsdll${_lib_suffix}.lib)
target_link_libraries (${PROJECT_NAME} gsdll)