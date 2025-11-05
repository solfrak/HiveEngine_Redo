cmake_minimum_required(VERSION 3.30.5)

# Create the test executable
add_executable(larvae_runner
    main.cpp
    memory/test_allocator.cpp
)

# Link against Larvae testing framework and modules to test
target_link_libraries(larvae_runner PRIVATE
    larvae
    hive
)

# Set output directory
set_target_properties(larvae_runner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Add test to CTest
enable_testing()
add_test(NAME AllTests COMMAND larvae_runner)
