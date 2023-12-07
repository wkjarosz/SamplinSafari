cmake_minimum_required(VERSION 3.13)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/" "${CMAKE_SOURCE_DIR}/resources")

include(VersionFromGit)

string(TIMESTAMP BUILD_TIME "%Y-%m-%d %H:%M")
message(STATUS "Saving build timestamp: ${BUILD_TIME}")

version_from_git(LOG ON TIMESTAMP "%Y-%m-%d-%H:%M:%S")

project(
  SamplinSafari
  DESCRIPTION "A research tool to visualize and interactively inspect high-dimensional (quasi) Monte Carlo samplers."
  VERSION ${VERSION}
  LANGUAGES C CXX
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(VERSION_LONG "${GIT_DESCRIBE} (64 bit)")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(VERSION_LONG "${GIT_DESCRIBE} (32 bit)")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/common.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/src/common.cpp @ONLY)

include(sanitizers)

# Set ourselves as the startup project in visual studio. Not available until cmake 3.6, but doesn't break older
# versions.
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT SamplinSafari)

include(CheckCXXCompilerFlag)

# ============================================================================
# Set a default build configuration (Release)
# ============================================================================
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Choose the type of build." FORCE
  )
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()
string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)

# ============================================================================
# Enable folders for projects in Visual Studio
# ============================================================================
if(CMAKE_GENERATOR MATCHES "Visual Studio")
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  # Prefer libc++ in conjunction with Clang
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    check_cxx_compiler_flag("-stdlib=libc++" HAS_LIBCPP)
    if(HAS_LIBCPP)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -D_LIBCPP_VERSION")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
      message(STATUS "SamplinSafari: using libc++.")
    else()
      message(
        WARNING
          "libc++ is recommended in conjunction with clang. Please insteall the libc++ development headers, provided e.g. by the packages 'libc++-dev' and 'libc++abi-dev' on Debian/Ubuntu."
      )
    endif()
  endif()

  # Enable link time optimization and set the default symbol visibility to hidden (very important to obtain small
  # binaries)
  if(NOT ${U_CMAKE_BUILD_TYPE} MATCHES DEBUG)
    # Default symbol visibility
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
    # set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  endif()

  # Disable specific GCC 7 warnings
  if(CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7.0)
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations -Wno-misleading-indentation -Wformat-truncation=0 -Wno-int-in-bool-context -Wimplicit-fallthrough=0"
    )
  endif()
endif()

# ============================================================================
# Sanitize build environment for static build with C++11/17
# ============================================================================
if(MSVC)
  # Disable annoying secure CRT warnings
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_CRT_SECURE_NO_WARNINGS")

  # Parallel build on MSVC (all targets)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

  if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE2")
  endif()

  # Static build
  set(CompilerFlags
      CMAKE_CXX_FLAGS
      CMAKE_CXX_FLAGS_DEBUG
      CMAKE_CXX_FLAGS_RELEASE
      CMAKE_CXX_FLAGS_MINSIZEREL
      CMAKE_CXX_FLAGS_RELWITHDEBINFO
      CMAKE_C_FLAGS
      CMAKE_C_FLAGS_DEBUG
      CMAKE_C_FLAGS_RELEASE
      CMAKE_C_FLAGS_MINSIZEREL
      CMAKE_C_FLAGS_RELWITHDEBINFO
  )
  foreach(CompilerFlag ${CompilerFlags})
    string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
  endforeach()
endif()

# ============================================================================
# Add dependencies via CPM (cmake/tools.cmake includes cmake/CPM.cmake)
#
# see https://github.com/TheLartians/CPM.cmake for more info
# ============================================================================
include(cmake/CPM.cmake)

CPMAddPackage("gh:wjakob/pcg32#70099eadb86d3999c38cf69d2c55f8adc1f7fe34")
if(pcg32_ADDED)
  add_library(pcg32 INTERFACE IMPORTED)
  target_include_directories(pcg32 INTERFACE "${pcg32_SOURCE_DIR}")
endif()

CPMAddPackage("gh:wkjarosz/galois#2bf22e85952c8612832838a26baf68ad93beff7a")
if(galois_ADDED)
  target_include_directories(galois++ INTERFACE "${galois_SOURCE_DIR}/include")
endif()

CPMAddPackage("gh:sgorsten/linalg@2.2")
if(linalg_ADDED)
  add_library(linalg INTERFACE IMPORTED)
  target_include_directories(linalg INTERFACE "${linalg_SOURCE_DIR}")
endif()

CPMAddPackage("gh:fmtlib/fmt#7.1.3")

CPMAddPackage("gh:samhocevar/portable-file-dialogs#7f852d88a480020d7f91957cbcefe514fc95000c")
if(portable_file_dialogs_ADDED)
  add_library(portable_file_dialogs INTERFACE IMPORTED)
  target_include_directories(portable_file_dialogs INTERFACE "${portable_file_dialogs_SOURCE_DIR}")
endif()

set(HELLOIMGUI_WITH_GLFW ON)
CPMAddPackage("gh:pthom/hello_imgui#c118a47790a4bbaa939621e07cd74906434f3df8")

# ============================================================================
# Compile remainder of the codebase with compiler warnings turned on
# ============================================================================
if(MSVC)
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -Wno-gnu-anonymous-struct -Wno-c99-extensions -Wno-nested-anon-types -Wno-deprecated-register"
    )
  endif()
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Compile samplers library
add_library(
  samplerlib OBJECT
  STATIC
  include/sampler/fwd.h
  include/sampler/Misc.h
  include/sampler/Sampler.h
  include/sampler/Halton.h
  include/sampler/halton_sampler.h
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
  include/sampler/RandomPermutation.h
  include/sampler/Sobol.h
  src/sampler/Misc.cpp
  src/sampler/Halton.cpp
  src/sampler/Jittered.cpp
  src/sampler/LP.cpp
  src/sampler/MultiJittered.cpp
  src/sampler/NRooks.cpp
  src/sampler/OA.cpp
  src/sampler/OAAddelmanKempthorne.cpp
  src/sampler/OABose.cpp
  src/sampler/OABoseBush.cpp
  src/sampler/OABush.cpp
  src/sampler/OACMJND.cpp
  src/sampler/SOA.cpp
  src/sampler/Random.cpp
  src/sampler/Sobol.cpp
  src/sampler/SobolMatrices.cpp
)

# Link dependencies
target_link_libraries(samplerlib PUBLIC galois++ pcg32)

set_target_properties(samplerlib PROPERTIES CXX_STANDARD 17)

# Now build the Samplin' Safari viewer app
string(TIMESTAMP YEAR "%Y")

set(output_name "Samplin Safari")
set(HELLO_IMGUI_BUNDLE_IDENTIFIER_URL_PART "com.im.SamplinSafari")
set(HELLO_IMGUI_BUNDLE_IDENTIFIER_NAME_PART ${app_name})
set(HELLO_IMGUI_ICON_DISPLAY_NAME ${output_name})
set(HELLO_IMGUI_BUNDLE_NAME ${output_name})
set(HELLO_IMGUI_BUNDLE_COPYRIGHT "© Wojciech Jarosz, ${YEAR}")
set(HELLO_IMGUI_BUNDLE_EXECUTABLE ${output_name})
set(HELLO_IMGUI_BUNDLE_VERSION ${VERSION})
set(HELLO_IMGUI_BUNDLE_SHORT_VERSION ${VERSION})
set(HELLO_IMGUI_BUNDLE_ICON_FILE icon.icns)

hello_imgui_add_app(
  SamplinSafari src/app.cpp ${CMAKE_CURRENT_BINARY_DIR}/src/common.cpp src/shader.cpp src/export_to_file.cpp
)

set_target_properties(SamplinSafari PROPERTIES OUTPUT_NAME ${output_name} CXX_STANDARD 17)

target_link_libraries(SamplinSafari PRIVATE samplerlib linalg fmt::fmt portable_file_dialogs)

if(UNIX AND NOT ${U_CMAKE_BUILD_TYPE} MATCHES DEBUG)
  add_custom_command(
    TARGET SamplinSafari
    POST_BUILD
    COMMAND strip $<TARGET_FILE:SamplinSafari>
  )
endif()

# Force colored output for the ninja generator
if(CMAKE_GENERATOR STREQUAL "Ninja")
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcolor-diagnostics")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdiagnostics-color=always")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
  endif()
endif()
