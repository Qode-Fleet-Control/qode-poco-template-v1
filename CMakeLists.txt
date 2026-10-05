# POCO consumed through its CMake package config, as POCO's own docs show:
# find_package(Poco REQUIRED <components>) and link the Poco::<component> targets.
cmake_minimum_required(VERSION 3.16)

project(app VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Poco REQUIRED COMPONENTS Foundation Net Util JSON)

add_executable(app src/main.cpp)
target_link_libraries(app PRIVATE Poco::Foundation Poco::Net Poco::Util Poco::JSON)

install(TARGETS app RUNTIME DESTINATION bin)
