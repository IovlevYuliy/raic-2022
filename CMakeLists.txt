cmake_minimum_required(VERSION 3.5)
set (CMAKE_CXX_STANDARD 17)
project(ai_cup_22)

# OS and compiler checks.
if(WIN32)
    add_definitions(-DWIN32)
    SET(PROJECT_LIBS Ws2_32.lib)
endif()

file(GLOB HEADERS "*.hpp" "include/*.hpp" "model/*.hpp" "stream/*.hpp" "codegame/*.hpp" "debugging/*.hpp")
file(GLOB SRC "*.cpp" "source/*.cpp" "model/*.cpp" "stream/*.cpp"  "codegame/*.cpp" "debugging/*.cpp")

SET_SOURCE_FILES_PROPERTIES(${HEADERS} PROPERTIES HEADER_FILE_ONLY TRUE)
include_directories("." "include")
add_executable(ai_cup_22 ${HEADERS} ${SRC})
TARGET_LINK_LIBRARIES(ai_cup_22 ${PROJECT_LIBS})