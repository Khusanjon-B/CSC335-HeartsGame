cmake_minimum_required(VERSION 3.16)
project(HeartsTestServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core    = event loop, strings, JSON
# Network = TCP sockets
# Sql     = database access (includes the built-in SQLite driver)
find_package(Qt6 REQUIRED COMPONENTS Core Network Sql)
qt_standard_project_setup()

qt_add_executable(test_server
    main.cpp
    Log.h
    Connection.h
    Database.h Database.cpp
    Server.h   Server.cpp
)
target_link_libraries(test_server PRIVATE Qt6::Core Qt6::Network Qt6::Sql)
