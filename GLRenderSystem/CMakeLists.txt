add_library(GLRenderSystem SHARED
  "GLRenderSystem.cpp"
  "GLRenderSystem.h"
  "GLWindow.cpp"
  "GLWindow.h"
  "Exports.h"
  "Exports.cpp"
  "Shader.h"
  "Shader.cpp"
  "VertexArray.h"
  "VertexArray.cpp"
  "Buffer.h"
  "Buffer.cpp"
  "VertexBuffer.h"
  "VertexBuffer.cpp"
  "IndexBuffer.h"
  "IndexBuffer.cpp"
  "utils/ShaderReader.cpp"
  "utils/glad.c"
  "../Interfaces/IWindow.cpp"
  "../Interfaces/IRenderSystem.cpp"
)

target_compile_features(GLRenderSystem PRIVATE cxx_std_20)

target_compile_definitions(GLRenderSystem PRIVATE OGL_RENDER_SYSTEM_EXPORT)

target_include_directories(GLRenderSystem PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/glad"
  "${CMAKE_CURRENT_SOURCE_DIR}/KHR"
  "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/glfw"
  "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/glm"
  "${CMAKE_CURRENT_SOURCE_DIR}/../Interfaces"
  "${CMAKE_CURRENT_SOURCE_DIR}/../MeshEditor"
)

if(WIN32)
  target_link_libraries(GLRenderSystem PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/glfw/glfw3.lib"
    comdlg32
    ole32
    uuid
    shell32
    opengl32
  )
else()
  target_link_libraries(GLRenderSystem PRIVATE
    ${CMAKE_DL_LIBS}
  )
endif()