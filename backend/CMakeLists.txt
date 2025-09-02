cmake_minimum_required(VERSION 3.15)
project(Concurrency LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Boost (asio is header-only inside Boost; find by required components system and thread)
find_package(Boost 1.70 REQUIRED COMPONENTS system thread)

if(NOT Boost_FOUND)
  message(FATAL_ERROR "Boost not found. Make sure you used vcpkg toolchain or integrated vcpkg.")
endif()

add_executable(server server.cpp) 

# Include Boost headers
target_include_directories(server PRIVATE ${Boost_INCLUDE_DIRS})

# Link required Boost libs (Boost.System, Boost.Thread). Use imported targets if available:
if(TARGET Boost::system)
  target_link_libraries(server PRIVATE Boost::system Boost::thread)
else()
  target_link_libraries(server PRIVATE ${Boost_LIBRARIES})
endif()

# On Windows you often need Winsock
if(WIN32)
  target_link_libraries(server PRIVATE ws2_32)
endif()
