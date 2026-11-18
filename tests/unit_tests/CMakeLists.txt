add_executable(unit_tests
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Decoder.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FramePacer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/VideoProcessor.cpp
)

target_include_directories(unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(unit_tests PRIVATE
    ${REELCAM_WIN_LIBS}
)
