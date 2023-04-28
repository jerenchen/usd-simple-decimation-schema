set(USD_PLUGIN_NAME trimeshImaging)

add_library(
    ${USD_PLUGIN_NAME} SHARED
        trimeshAdapter.cpp
)

target_include_directories(
    ${USD_PLUGIN_NAME} PRIVATE
        ${PXR_INCLUDE_DIRS}
)

set_target_properties(
    ${USD_PLUGIN_NAME} PROPERTIES
        PREFIX ""
        INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/plugin;${PXR_CMAKE_DIR}/lib"
        MACOSX_RPATH TRUE
)

target_compile_features(
    ${USD_PLUGIN_NAME} PUBLIC
        cxx_auto_type
)

target_link_libraries(
    ${USD_PLUGIN_NAME}
        ${PXR_LIBRARIES}
        igl::core
)

install(
    TARGETS ${USD_PLUGIN_NAME}
        LIBRARY DESTINATION plugin
)

set(PLUG_INFO_ROOT "..")
set(PLUG_INFO_LIBRARY_PATH "../${USD_PLUGIN_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(PLUG_INFO_RESOURCE_PATH "resources")

configure_file(
    plugInfo.json
        ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json
        @ONLY
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json
        DESTINATION plugin/${USD_PLUGIN_NAME}/resources
)