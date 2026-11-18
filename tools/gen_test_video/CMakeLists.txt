add_executable(gen_test_video main.cpp)

target_include_directories(gen_test_video PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
)

target_link_libraries(gen_test_video PRIVATE
    ${REELCAM_WIN_LIBS}
)
