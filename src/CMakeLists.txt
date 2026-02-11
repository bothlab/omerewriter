
qt_add_executable(OMERewriter
        WIN32 MACOSX_BUNDLE
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        microscopeparamswidget.h
        microscopeparamswidget.cpp
        microscopeparamswidget.ui
        imageviewwidget.h
        imageviewwidget.cpp
)

target_include_directories(OMERewriter SYSTEM PRIVATE
        "${omefiles_SOURCE_DIR}/lib"
        "${omefiles_BINARY_DIR}/lib"
)
target_link_libraries(OMERewriter
        PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::OpenGLWidgets
        OME::Files
)



install(TARGETS OMERewriter
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
        TARGET OMERewriter
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
