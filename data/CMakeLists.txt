
install(FILES
        "io.github.bothlab.omerewriter.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
)

install(FILES
        "omerewriter.svg"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
)

install(FILES
        "io.github.bothlab.omerewriter.metainfo.xml"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo"
)
