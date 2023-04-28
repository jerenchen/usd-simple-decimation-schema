set(USD_PLUGIN_NAME trimesh)

set(PLUG_INFO_ROOT "..")
set(PLUG_INFO_LIBRARY_PATH "../${USD_PLUGIN_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(PLUG_INFO_RESOURCE_PATH "resources")

configure_file(
    plugInfo.json
        ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json
        @ONLY
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json generatedSchema.usda
        DESTINATION plugin/${USD_PLUGIN_NAME}/resources
)
