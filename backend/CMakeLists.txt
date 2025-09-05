cmake_minimum_required(VERSION 3.16)
set(CMAKE_TOOLCHAIN_FILE "C:/Users/wujas/Desktop/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

project(server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Boost 1.70 REQUIRED COMPONENTS system thread)

add_executable(server
  server.cpp
  sessionmanager.cpp
  database.cpp
)

if (WIN32)
  # Target Windows 7 compatibility. Change to 0x0A00 for Windows 10 if you prefer.
  target_compile_definitions(server PRIVATE _WIN32_WINNT=0x0601)
endif()
# Ensure Boost headers (for header-only asio) are visible
target_include_directories(server PRIVATE ${Boost_INCLUDE_DIRS})

target_link_libraries(server PRIVATE
  Boost::system
  Boost::thread
  SQLite::SQLite3
  Threads::Threads
  nlohmann_json::nlohmann_json
)

# Put the built binary under /app/bin/ when we "cmake --install"
install(TARGETS server RUNTIME DESTINATION bin)
