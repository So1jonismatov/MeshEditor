
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)

file(GLOB_RECURSE MeshEditorSources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/tinyxml2/tinyxml2.cpp")

list(FILTER MeshEditorSources EXCLUDE REGEX "[/\\](out|\.vs)[/\\]")
list(REMOVE_DUPLICATES MeshEditorSources)

add_executable(MeshEditor ${MeshEditorSources}
    "${CMAKE_CURRENT_SOURCE_DIR}/../Interfaces/IWindow.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Interfaces/IRenderSystem.cpp")

target_compile_features(MeshEditor PRIVATE cxx_std_20)

if(MSVC)
    set_target_properties(MeshEditor PROPERTIES
        LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
        LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
        LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
        LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE"
    )
endif()

target_include_directories(MeshEditor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/View"
    "${CMAKE_CURRENT_SOURCE_DIR}/Model"
    "${CMAKE_CURRENT_SOURCE_DIR}/Model/Geometry"
    "${CMAKE_CURRENT_SOURCE_DIR}/Model/Graph"
    "${CMAKE_CURRENT_SOURCE_DIR}/Model/Manipulators"
    "${CMAKE_CURRENT_SOURCE_DIR}/Model/IO"
    "${CMAKE_CURRENT_SOURCE_DIR}/Operators"
    "${CMAKE_CURRENT_SOURCE_DIR}/Operators/Camera"
    "${CMAKE_CURRENT_SOURCE_DIR}/Operators/Display"
    "${CMAKE_CURRENT_SOURCE_DIR}/Operators/Editing"
    "${CMAKE_CURRENT_SOURCE_DIR}/Operators/IO"
    "${CMAKE_CURRENT_SOURCE_DIR}/Operators/Selection"
    "${CMAKE_CURRENT_SOURCE_DIR}/Operators/Topology"
    "${CMAKE_CURRENT_SOURCE_DIR}/QtUI"
    "${CMAKE_CURRENT_SOURCE_DIR}/QtUI/Adapters"
    "${CMAKE_CURRENT_SOURCE_DIR}/QtUI/Console"
    "${CMAKE_CURRENT_SOURCE_DIR}/QtUI/Dialogs"
    "${CMAKE_CURRENT_SOURCE_DIR}/QtUI/Widgets"
    "${CMAKE_CURRENT_SOURCE_DIR}/Threading"
    "${CMAKE_CURRENT_SOURCE_DIR}/../HalfEdge"
    "${CMAKE_CURRENT_SOURCE_DIR}/../GLRenderSystem"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/glfw"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/glm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/tinyxml2"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/stb"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Interfaces"
)

target_link_libraries(MeshEditor PRIVATE
    HalfEdge
    Qt6::Widgets
    Qt6::OpenGLWidgets
    TBB::tbb
)

if(WIN32)
    target_link_libraries(MeshEditor PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/glfw/glfw3.lib"
    )
else()
    target_link_libraries(MeshEditor PRIVATE
        ${CMAKE_DL_LIBS}
    )
endif()

add_custom_command(TARGET MeshEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../GLRenderSystem/shaders"
        "$<TARGET_FILE_DIR:MeshEditor>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:GLRenderSystem>"
        "$<TARGET_FILE_DIR:MeshEditor>"
    COMMENT "Copying GLRenderSystem.dll to MeshEditor output directory"
)

# Copy the Qt runtime next to the freshly built exe so it can be launched
# straight from the build tree (debugging from the IDE included).
find_program(WINDEPLOYQT_EXECUTABLE windeployqt
    HINTS "${Qt6_DIR}/../../../bin")

if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET MeshEditor POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations
            "$<TARGET_FILE:MeshEditor>"
        COMMENT "Running windeployqt on the build output"
    )

    # `cmake --build <dir> --config Release --target deploy` produces a fully
    # self-contained, movable folder: exe + GLRenderSystem.dll + shaders +
    # all required Qt/compiler runtime DLLs.
    set(MESHEDITOR_DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy")
    add_custom_target(deploy
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MESHEDITOR_DEPLOY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:MeshEditor>" "${MESHEDITOR_DEPLOY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:GLRenderSystem>" "${MESHEDITOR_DEPLOY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/../GLRenderSystem/shaders"
            "${MESHEDITOR_DEPLOY_DIR}/shaders"
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --compiler-runtime
            "${MESHEDITOR_DEPLOY_DIR}/$<TARGET_FILE_NAME:MeshEditor>"
        DEPENDS MeshEditor GLRenderSystem
        COMMENT "Deploying MeshEditor to ${MESHEDITOR_DEPLOY_DIR}"
    )
else()
    message(WARNING "windeployqt not found - the deploy target is unavailable")
endif()
