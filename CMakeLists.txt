cmake_minimum_required(VERSION 3.10)

project(nationalrail-opendata-cpp-example)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package(ActiveMQ-CPP REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(run src/main.cpp)
target_link_libraries(run ActiveMQ-CPP::activemq-cpp ZLIB::ZLIB)
