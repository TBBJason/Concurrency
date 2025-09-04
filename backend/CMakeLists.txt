cmake_minimum_required(VERSION 3.16)
project(server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Boost 1.70 REQUIRED COMPONENTS system thread)

add_executable(server
  server.cpp
  sessionmanager.cpp
  database.cpp
)

# Ensure Boost headers (for header-only asio) are visible
target_include_directories(server PRIVATE ${Boost_INCLUDE_DIRS})

target_link_libraries(server PRIVATE
  Boost::system
  Boost::thread
  SQLite::SQLite3
  Threads::Threads
)

# Put the built binary under /app/bin/ when we "cmake --install"
install(TARGETS server RUNTIME DESTINATION bin)
