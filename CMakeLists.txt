cmake_minimum_required(VERSION 3.0.0)
project(Reliable_UDP)

# Add Executable for Server
add_executable(Server Server.cpp)
# Link Winsock Library
target_link_libraries(Server ws2_32)

# Add Executable for Client
add_executable(Client Client.cpp)
# Link Winsock Library
target_link_libraries(Client ws2_32)
