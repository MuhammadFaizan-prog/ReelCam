cmake_minimum_required(VERSION 3.20)
project(ReelCam VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC specific flags
if(MSVC)
    add_compile_options(/W4 /WX /utf-8 /permissive-)
    add_compile_definitions(
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Windows SDK libraries needed throughout
set(REELCAM_WIN_LIBS
    mfplat.lib
    mfuuid.lib
    mf.lib
    mfreadwrite.lib
    ole32.lib
    oleaut32.lib
    uuid.lib
    shlwapi.lib
    d3d11.lib
    dxgi.lib
)

add_subdirectory(src/mfsource)
add_subdirectory(src/core)
add_subdirectory(src/ui)
add_subdirectory(tools/cam_register)
add_subdirectory(tools/cam_test_enum)
add_subdirectory(tools/gen_test_video)
add_subdirectory(tests/unit_tests)

# End-to-end validation against the registered Windows virtual camera.
add_executable(test_vcam_stream tools/test_vcam_stream.cpp)
target_link_libraries(test_vcam_stream PRIVATE ${REELCAM_WIN_LIBS})
