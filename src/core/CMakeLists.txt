add_executable(reelcam_core
    main.cpp
    CoreServer.cpp
    CoreServer.h
    Decoder.cpp
    Decoder.h
    FramePacer.cpp
    FramePacer.h
    VideoProcessor.cpp
    VideoProcessor.h
    WebcamPassthrough.cpp
    WebcamPassthrough.h
)

target_include_directories(reelcam_core PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
)

target_link_libraries(reelcam_core PRIVATE
    ${REELCAM_WIN_LIBS}
)
