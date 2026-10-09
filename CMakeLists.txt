cmake_minimum_required(VERSION 3.10)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

 project("lab6")

find_package(TBB REQUIRED)

# Convenience default so a plain configure finds the installed Qt kit; any
# explicitly provided CMAKE_PREFIX_PATH / Qt6_DIR wins.
if(WIN32 AND NOT DEFINED Qt6_DIR AND EXISTS "C:/Qt/6.11.1/msvc2022_64")
    list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.11.1/msvc2022_64")
endif()

add_subdirectory("HalfEdge")
add_subdirectory("GLRenderSystem")
add_subdirectory("MeshEditor")