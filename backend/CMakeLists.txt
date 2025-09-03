cmake_minimum_required(VERSION 3.15)
project(Concurrency LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional: prefer dynamic libs (adjust if you want static)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

# Find Boost (we need system and thread)
find_package(Boost 1.70 REQUIRED COMPONENTS system thread)

if(NOT Boost_FOUND)
  message(FATAL_ERROR "Boost not found. Make sure vcpkg toolchain is configured if you installed Boost via vcpkg.")
endif()

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Recommended: find threads portably
find_package(Threads REQUIRED)

add_executable(server
  server.cpp
  sessionmanager.cpp
  database.cpp
)

# Include Boost headers
target_include_directories(server PRIVATE ${Boost_INCLUDE_DIRS})

# Link SQLite3
target_link_libraries(server PRIVATE SQLite::SQLite3)

# Define Windows target macro so Asio/Beast compiles cleanly
if(WIN32)
  target_compile_definitions(server PRIVATE _WIN32_WINNT=0x0601) # Win7; change to 0x0A00 for Windows 10
  # link Winsock
  target_link_libraries(server PRIVATE ws2_32)
else()
  # On POSIX, link pthread via Threads::Threads
  target_link_libraries(server PRIVATE Threads::Threads)
endif()

# Link Boost libs (use imported targets if available, otherwise fallback to ${Boost_LIBRARIES})
if (TARGET Boost::system AND TARGET Boost::thread)
  target_link_libraries(server PRIVATE Boost::system Boost::thread)
else()
  target_link_libraries(server PRIVATE ${Boost_LIBRARIES})
endif()

# If you later use Beast SSL/TLS, you will need OpenSSL:
# find_package(OpenSSL REQUIRED)
# target_link_libraries(server PRIVATE OpenSSL::SSL OpenSSL::Crypto)