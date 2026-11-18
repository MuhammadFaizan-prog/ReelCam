add_executable(cam_register main.cpp)

target_include_directories(cam_register PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
)

target_link_libraries(cam_register PRIVATE
    ole32.lib
    oleaut32.lib
    advapi32.lib
    mfplat.lib
)
