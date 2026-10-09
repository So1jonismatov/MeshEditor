add_library(HalfEdge STATIC
    "HalfEdge.cpp"
    "HalfEdge.h"
)

target_compile_features(HalfEdge PRIVATE cxx_std_20)

target_include_directories(HalfEdge PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/glm"
)
