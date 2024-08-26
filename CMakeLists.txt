cmake_minimum_required(VERSION 3.10)

project(fsnotify)

ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

# specify the C++ standard
set(SUPPORTED_CXX_STANDARDS 14 17)
if(NOT DEFINED CMAKE_CXX_STANDARD)
  message(STATUS "Setting C++ version to '14' as none was specified.")
  set(CMAKE_CXX_STANDARD 14)
else()
  message(STATUS "CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD} set by parent project.")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(
    "-Wall" "-Wpedantic" "-Wextra" "-fexceptions" "-fstandalone-debug"
    "$<$<CONFIG:DEBUG>:-O0;-g3;-ggdb>"
  )
else()
  add_compile_options(
    "-Wall" "-Wpedantic" "-Wextra" "-fexceptions"
    "$<$<CONFIG:DEBUG>:-O0;-g3;-ggdb>"
  )
endif()

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/SrvLib)

add_subdirectory(SrvLib EXCLUDE_FROM_ALL)

file(READ "FsNotify.cpp" SrcFile)
string(REGEX MATCH "szSrvName = L\"([^\"]*)\"" _ ${SrcFile})
set(SrvName ${CMAKE_MATCH_1})
message("ServiceName: ${SrvName}")

if ("${SrvName}" STREQUAL "")
  set(SrvName ${PROJECT_NAME})
endif()

set(targetSrc
        ${CMAKE_CURRENT_LIST_DIR}/FsNotify.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ConfFile.cpp
)

add_executable(${PROJECT_NAME}  ${targetSrc})
target_link_libraries(${PROJECT_NAME} srvlib pthread)

if (NOT EXISTS(${CMAKE_CURRENT_BINARY_DIR}/${SrvName}srv))
  file(READ SrvLib/init.d/examplesrv FILE_CONTENTS)
  string(REPLACE "ExampleSrv" ${SrvName} FILE_CONTENTS ${FILE_CONTENTS})
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${SrvName}srv ${FILE_CONTENTS})
endif()

if (NOT EXISTS(${CMAKE_CURRENT_BINARY_DIR}/${SrvName}.service))
  file(READ SrvLib/example.service FILE_CONTENTS)
  string(REPLACE "ExampleSrv" ${SrvName} FILE_CONTENTS ${FILE_CONTENTS})
  string(REPLACE "~" ${CMAKE_SOURCE_DIR} FILE_CONTENTS ${FILE_CONTENTS})
  string(REPLACE "Some Example server" "Notify Service" FILE_CONTENTS ${FILE_CONTENTS})
  string(REPLACE "example/" "" FILE_CONTENTS ${FILE_CONTENTS})
  string(REPLACE "example" "" FILE_CONTENTS ${FILE_CONTENTS})
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${SrvName}.service ${FILE_CONTENTS})
endif()
