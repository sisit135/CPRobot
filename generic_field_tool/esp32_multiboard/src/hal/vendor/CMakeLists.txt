# Reuse the existing vendor checkout without copying or changing vendor sources.
get_filename_component(TOOL_REPO "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
set(EMBEDDED_PROTO "${TOOL_REPO}/third_party/EmbeddedProto/src")
add_library(tool_embedded_proto STATIC "${EMBEDDED_PROTO}/Fields.cpp"
    "${EMBEDDED_PROTO}/MessageInterface.cpp" "${EMBEDDED_PROTO}/ReadBufferSection.cpp")
target_include_directories(tool_embedded_proto PUBLIC "${EMBEDDED_PROTO}")
target_compile_features(tool_embedded_proto PUBLIC cxx_std_17)
