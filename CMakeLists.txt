cmake_minimum_required(VERSION 3.13)
project(codecrafters-shell)

set(CMAKE_C_STANDARD 23)

file(GLOB SOURCE_FILES CONFIGURE_DEPENDS src/*.c)
add_executable(shell ${SOURCE_FILES})

if (NOT WIN32)
  target_link_libraries(shell PRIVATE readline)
endif()

