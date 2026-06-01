
# Core TIFF/OME-XML I/O and metadata model, shared by the GUI app and the CLI.
add_library(omerw_core STATIC
        ometiffimage.h
        ometiffimage.cpp
        ometypes.h
        tifffile.h
        tifffile.cpp
        omexml.h
        omexml.cpp
        ometiffreader.h
        ometiffreader.cpp
        ometiffwriter.h
        ometiffwriter.cpp
        metadatajson.h
        metadatajson.cpp
)
target_include_directories(omerw_core PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}/src"
)
target_link_libraries(omerw_core
        PUBLIC
        Qt::Core
        Qt::Xml
        TIFF::TIFF
)

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
        savedparamsmanager.h
        savedparamsmanager.cpp
        utils.h
        utils.cpp
        resources.qrc
        $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_BINARY_DIR}/omerewriter.rc>
)

target_include_directories(OMERewriter PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}/src"
)
target_link_libraries(OMERewriter
        PRIVATE
        omerw_core
        Qt::Core
        Qt::Widgets
        Qt::Svg
        Qt::OpenGLWidgets
        Qt::Xml
        TIFF::TIFF
)

# Headless CLI for inspecting and rewriting OME-TIFF files (handy for testing).
qt_add_executable(omerw-cli
        cli.cpp
)
target_link_libraries(omerw-cli PRIVATE omerw_core Qt::Core)
install(TARGETS omerw-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS OMERewriter
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(WIN32)
    # Convert the SVG icon to a multi-resolution ICO file for the Windows executable.
    find_program(RSVG_CONVERT rsvg-convert REQUIRED)
    find_program(ICOTOOL icotool REQUIRED)

    set(_svg_src "${CMAKE_SOURCE_DIR}/data/omerewriter.svg")
    set(_ico_out "${CMAKE_CURRENT_BINARY_DIR}/omerewriter.ico")

    # Generate the .rc file into the build directory, pointing at the build-dir ICO.
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/omerewriter.rc"
         "IDI_ICON1 ICON \"${_ico_out}\"\n")

    foreach(_sz 16 32 48 256)
        list(APPEND _png_files "${CMAKE_CURRENT_BINARY_DIR}/icon_${_sz}.png")
        add_custom_command(
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/icon_${_sz}.png"
            COMMAND ${RSVG_CONVERT} -w ${_sz} -h ${_sz} -o "${CMAKE_CURRENT_BINARY_DIR}/icon_${_sz}.png" "${_svg_src}"
            DEPENDS "${_svg_src}"
            COMMENT "Rendering icon at ${_sz}×${_sz}"
        )
    endforeach()
    add_custom_command(
        OUTPUT "${_ico_out}"
        COMMAND ${ICOTOOL} -c -o "${_ico_out}" ${_png_files}
        DEPENDS ${_png_files}
        COMMENT "Creating omerewriter.ico"
    )

    # Make the executable depend on the ICO (and thus the RC) being generated
    add_custom_target(omerewriter_ico DEPENDS "${_ico_out}")
    add_dependencies(OMERewriter omerewriter_ico)

    set_target_properties(OMERewriter PROPERTIES
        VS_DPI_AWARE "PerMonitor"
    )

    install(
        FILES "${_ico_out}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/"
    )

    qt_generate_deploy_app_script(
        TARGET OMERewriter
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()
