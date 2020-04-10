cmake_minimum_required(VERSION 3.5)

project(server LANGUAGES CXX)

find_package(Boost COMPONENTS system REQUIRED)
find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME} main.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/common)

target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads Boost::system)
