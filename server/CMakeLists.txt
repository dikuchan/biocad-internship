cmake_minimum_required(VERSION 3.15)

project(server)
add_executable(server open62541.c server.c)

set_property(TARGET server PROPERTY C_STANDARD 11)

# Optionally open62541 header could be generated and used as the static library.
target_link_libraries(server PRIVATE open62541)
