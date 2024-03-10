cmake_minimum_required(VERSION 3.22)

project(unopp_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

aux_source_directory(src DIR_SRCS)

set(EXECUTABLE_OUTPUT_PATH ./build)

add_subdirectory(SQLiteCpp)

add_executable(${PROJECT_NAME} ${DIR_SRCS})

target_include_directories(
    ${PROJECT_NAME} PUBLIC
    ./include
)

target_link_libraries(
    ${PROJECT_NAME} 
    SQLiteCpp
)