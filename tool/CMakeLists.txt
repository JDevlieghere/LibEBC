project (tool)
add_subdirectory(vendor)

# Configure
configure_file (
  "${tool_SOURCE_DIR}/ebcutil.cpp"
  "${tool_BINARY_DIR}/ebcutil.cpp")

add_executable(ebcutil ${tool_BINARY_DIR}/ebcutil.cpp)
target_link_libraries(ebcutil ebc tclap rang)

set_property(TARGET ebcutil PROPERTY CXX_STANDARD 14)
