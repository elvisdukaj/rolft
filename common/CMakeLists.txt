cmake_minimum_required(VERSION 3.10)

project(common LANGUAGES CXX)

find_package(Boost COMPONENTS system REQUIRED )
find_package(Threads REQUIRED)
find_package(fmt REQUIRED)

add_library(${PROJECT_NAME} main.cpp)
add_library(rolft::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/common)

target_link_libraries(${PROJECT_NAME}
PRIVATE
  Threads::Threads
  Boost::system
  fmt::fmt-header-only
)
