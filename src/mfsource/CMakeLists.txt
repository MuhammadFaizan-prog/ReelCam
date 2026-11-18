add_library(mfsource SHARED
    dllmain.cpp
    ClassFactory.cpp
    ReelCamMediaSource.cpp
    ReelCamMediaStream.cpp
)

target_include_directories(mfsource PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/mfsource
)

target_link_libraries(mfsource PRIVATE
    ${REELCAM_WIN_LIBS}
    kernel32.lib
    user32.lib
    advapi32.lib
)

# Export only the COM entry points
set_target_properties(mfsource PROPERTIES
    OUTPUT_NAME "mfsource"
    PREFIX ""
    DEFINE_SYMBOL "MFSOURCE_EXPORTS"
)

# Use the .def file for explicit exports
target_sources(mfsource PRIVATE mfsource.def)
