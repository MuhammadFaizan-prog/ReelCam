add_executable(cam_test_enum main.cpp)

target_include_directories(cam_test_enum PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
)

target_link_libraries(cam_test_enum PRIVATE
    ${REELCAM_WIN_LIBS}
)
