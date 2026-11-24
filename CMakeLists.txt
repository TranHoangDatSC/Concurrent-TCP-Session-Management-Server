
cmake_minimum_required(VERSION 3.16)

project(CTSMS
    VERSION 1.0
    DESCRIPTION "Concurrent TCP Session Management Server"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include project headers
include_directories(${PROJECT_SOURCE_DIR}/include)

# Build the protocol module
add_library(ctsms_protocol
    src/protocol.cpp
)

# Build the protocol tests
add_executable(protocol_tests
    tests/protocol_tests.cpp
)

target_link_libraries(protocol_tests
    PRIVATE ctsms_protocol
)

# Enable testing
enable_testing()
add_test(NAME ProtocolTests COMMAND protocol_tests)
