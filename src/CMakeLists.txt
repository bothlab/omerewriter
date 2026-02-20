
qt_add_executable(OMERewriter
        WIN32 MACOSX_BUNDLE
        main.cpp
        mainwindow.h
        mainwindow.cpp
        mainwindow.ui
        microscopeparamswidget.h
        microscopeparamswidget.cpp
        microscopeparamswidget.ui
        elidedlabel.h
        elidedlabel.cpp
        imageviewwidget.h
        imageviewwidget.cpp
        rangeslider.h
        rangeslider.cpp
        ometiffimage.h
        ometiffimage.cpp
        metadatajson.h
        metadatajson.cpp
        savedparamsmanager.h
        savedparamsmanager.cpp
)

target_include_directories(OMERewriter SYSTEM PRIVATE
        "${omefiles_SOURCE_DIR}/lib"
        "${omefiles_BINARY_DIR}/lib"
)
target_include_directories(OMERewriter PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
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
