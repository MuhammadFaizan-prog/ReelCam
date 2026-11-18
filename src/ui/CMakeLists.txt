add_executable(reelcam_ui WIN32
    main.cpp
    MainWindow.cpp
    MainWindow.h
)

target_include_directories(reelcam_ui PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
)

target_link_libraries(reelcam_ui PRIVATE
    ${REELCAM_WIN_LIBS}
    comctl32.lib
    dwmapi.lib
    shell32.lib
    comdlg32.lib
)
