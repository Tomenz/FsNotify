cmake_minimum_required(VERSION 3.10)

project(dirnotify)

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

set(targetSrc
        ${CMAKE_CURRENT_LIST_DIR}/DirNotify.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ConfFile.cpp
)

add_executable(${PROJECT_NAME}  ${targetSrc})
target_link_libraries(${PROJECT_NAME} srvlib pthread)

file(READ SrvLib/init.d/examplesrv FILE_CONTENTS)
string(REPLACE "ExampleSrv" ${PROJECT_NAME} NEW_FILE_CONTENTS ${FILE_CONTENTS})
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}srv ${NEW_FILE_CONTENTS})

file(READ SrvLib/example.service FILE_CONTENTS)
string(REPLACE "ExampleSrv" ${PROJECT_NAME} NEW_FILE_CONTENTS ${FILE_CONTENTS})
string(REPLACE "~" ${CMAKE_CURRENT_BINARY_DIR} NEW_FILE_CONTENTS ${NEW_FILE_CONTENTS})
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.service ${NEW_FILE_CONTENTS})
