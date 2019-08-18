cmake_minimum_required (VERSION 2.8.3)
project(SamplinSafari)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# prevent glfw from changing the directory on macOS bundles
SET(GLFW_USE_CHDIR OFF CACHE BOOL "Prevent glfwInit from chdir to Contents/Resources" FORCE)

# Set a default build configuration (Release)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()
string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)

# This is useful when compiling with Ninja. When compiling with clang or gcc,
# this will force it to output well-formatted/colored output.
option (FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." FALSE)
if (${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
       add_compile_options (-fdiagnostics-color=always)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       add_compile_options (-fcolor-diagnostics)
    endif ()
endif ()

if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/ext/nanogui/ext/glfw")
  message(FATAL_ERROR "The SamplinSafari dependency repositories (NanoGUI, GLFW, etc.) are missing! "
    "You probably did not clone the project with --recursive. It is possible to recover "
    "by calling \"git submodule update --init --recursive\"")
endif()

# Enable folders for projects in Visual Studio
if (CMAKE_GENERATOR MATCHES "Visual Studio")
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

# Sanitize build environment for static build with C++11
if (MSVC)
  # Disable annoying secure CRT warnings
  add_definitions (/D "_CRT_SECURE_NO_WARNINGS")

  # Parallel build on MSVC (all targets)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

  if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE2")

    # Disable Eigen vectorization for Windows 32 bit builds (issues with unaligned access segfaults)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DEIGEN_DONT_ALIGN")
  endif()

  # Static build
  set(CompilerFlags
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
  foreach(CompilerFlag ${CompilerFlags})
    string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
  endforeach()
elseif(APPLE)
    # Try to auto-detect a suitable SDK
    # Commented out for now -- causes a too new SDK to be selected on AppVeyor
    #execute_process(COMMAND bash -c "xcodebuild -version -sdk | grep MacOSX | grep Path | head -n 1 | cut -f 2 -d ' '" OUTPUT_VARIABLE CMAKE_OSX_SYSROOT)
    #string(REGEX REPLACE "(\r?\n)+$" "" CMAKE_OSX_SYSROOT "${CMAKE_OSX_SYSROOT}")
    #string(REGEX REPLACE "^.*X([0-9.]*).sdk$" "\\1" CMAKE_OSX_DEPLOYMENT_TARGET "${CMAKE_OSX_SYSROOT}")
    add_definitions(-DGL_SILENCE_DEPRECATION)
endif()

# Enable C++11 mode on GCC / Clang
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

# Build Galois++ library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ext/galois++ ext_build/galois++)

# Build NanoGUI
set(NANOGUI_BUILD_EXAMPLE OFF CACHE BOOL " " FORCE)
set(NANOGUI_BUILD_SHARED  OFF CACHE BOOL " " FORCE)
set(NANOGUI_BUILD_PYTHON  OFF CACHE BOOL " " FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ext/nanogui ext_build/nanogui)
set_property(TARGET glfw glfw_objects nanogui galois++ PROPERTY FOLDER "dependencies")

# Compile remainder of the codebase with compiler warnings turned on
if(MSVC)
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-anonymous-struct -Wno-c99-extensions -Wno-nested-anon-types -Wno-deprecated-register")
  endif()
endif()

include_directories(
    # NanoGUI
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/nanogui/include
    # NanoGUI dependencies (Eigen, GLFW, NanoVG, GLAD?)
    ${NANOGUI_EXTRA_INCS}
    # tinyformat
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/tinyformat
    # pcg32
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/pcg32
    # galois++
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/galois++/include
    # sampler
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Compile samplers library
add_library(samplerlib OBJECT STATIC
  include/sampler/Misc.h
  include/sampler/Sampler.h
  include/sampler/Halton.h
  include/sampler/Hammersley.h
  include/sampler/Jittered.h
  include/sampler/LP.h
  include/sampler/MultiJittered.h
  include/sampler/NRooks.h
  include/sampler/OA.h
  include/sampler/OAAddelmanKempthorne.h
  include/sampler/OABose.h
  include/sampler/OABoseBush.h
  include/sampler/OABush.h
  include/sampler/OACMJND.h
  include/sampler/SOA.h
  include/sampler/Random.h
  include/sampler/Sobol.h
  src/Misc.cpp
  src/Halton.cpp
  src/Jittered.cpp
  src/LP.cpp
  src/MultiJittered.cpp
  src/NRooks.cpp
  src/OA.cpp
  src/OAAddelmanKempthorne.cpp
  src/OABose.cpp
  src/OABoseBush.cpp
  src/OABush.cpp
  src/OACMJND.cpp
  src/SOA.cpp
  src/Random.cpp
  src/Sobol.cpp
  src/SobolMatrices.cpp)
target_link_libraries(samplerlib galois++)

# Resource file (icons etc.)
set(EXTRA_SOURCE "")
if (APPLE)
  set(EXTRA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.icns")
elseif(WIN32)
  #set(EXTRA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.rc")
endif()

# The following lines build the main executable. If you add a source
# code file to Nori, be sure to include it in this list.
add_executable(SamplinSafari MACOSX_BUNDLE
    gui/SampleViewer.cpp
    gui/SampleViewer.h
    gui/main.cpp
    gui/Well.cpp
    gui/Well.h
    ${EXTRA_SOURCE}
)

add_definitions(${NANOGUI_EXTRA_DEFS})
target_link_libraries(SamplinSafari nanogui ${NANOGUI_EXTRA_LIBS} galois++ samplerlib)

set_target_properties(SamplinSafari PROPERTIES OUTPUT_NAME "SamplinSafari")

if (APPLE)
  # Build an application bundle on OSX
  set_target_properties(SamplinSafari PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "SamplinSafari")
  set_target_properties(SamplinSafari PROPERTIES MACOSX_BUNDLE_BUNDLE_GUI_IDENTIFIER "com.im.SamplinSafari")
  set_target_properties(SamplinSafari PROPERTIES MACOSX_BUNDLE_ICON_FILE icon.icns)
  set_target_properties(SamplinSafari PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/resources/MacOSXBundleInfo.plist.in)
  set_source_files_properties(resources/icon.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
else()
  # Insulate from a few types of ABI changes by statically linking against libgcc and libstdc++
  set_target_properties(SamplinSafari PROPERTIES LINK_FLAGS "-static-libgcc")
endif()

if (UNIX AND NOT ${U_CMAKE_BUILD_TYPE} MATCHES DEBUG)
  add_custom_command(TARGET SamplinSafari POST_BUILD COMMAND strip $<TARGET_FILE:SamplinSafari>)
endif()



#============================================================================
# Check for doxygen and provide a make target to build the documentation
#============================================================================

# first we can indicate the documentation build as an option and set it to ON by default
option(BUILD_DOC "Build documentation" ON)

# check if Doxygen is installed
find_package(Doxygen)
if (DOXYGEN_FOUND)
    # set input and output files
    set(DOXYGEN_IN ${CMAKE_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_BINARY_DIR}/doc/Doxyfile)

    # request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    message("Configuring doxygen and adding \"doc\" make target")

    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/doc/
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM )
else (DOXYGEN_FOUND)
    message("Unable to find Doxygen, so doxygen documentation won't be built")
endif (DOXYGEN_FOUND)