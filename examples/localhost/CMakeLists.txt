cmake_minimum_required(VERSION 3.11)

project(localhost
    LANGUAGES CXX
)

add_executable(localhost_server localhost_server.cpp)
target_link_libraries(localhost_server
    PRIVATE trftp::trftp-server
)

add_executable(localhost_client localhost_client.cpp)
target_link_libraries(localhost_client
    PRIVATE trftp::trftp-client
)