
install(FILES
        "io.github.bothlab.omerewriter.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
)

install(FILES
        "omerewriter.svg"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
)

# AppStream metainfo data
set(METAINFO_FILENAME_SRC "io.github.bothlab.omerewriter.metainfo.xml")
find_program(ASCLI_EXE appstreamcli)
if (ASCLI_EXE)
# add release info and install metadata
set(METAINFO_FILE_REL "${CMAKE_CURRENT_BINARY_DIR}/${METAINFO_FILENAME_SRC}")
add_custom_command(OUTPUT ${METAINFO_FILE_REL}
    COMMAND ${ASCLI_EXE}
            news-to-metainfo --limit=4 --format=text
            "${CMAKE_SOURCE_DIR}/NEWS.md"
            ${METAINFO_FILENAME_SRC}
            ${METAINFO_FILE_REL}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${METAINFO_FILENAME_SRC}"
    COMMENT "Adding release info to metainfo file"
    VERBATIM
)
add_custom_target(metainfo_add_release ALL DEPENDS ${METAINFO_FILE_REL})
install(FILES ${METAINFO_FILE_REL} DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")

# validate our metadata
add_test(NAME metainfo-validate
    COMMAND ${ASCLI_EXE} validate --pedantic --no-net ${METAINFO_FILE_REL}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
else()
    # if we have no appstreamcli, we just install the unprocessed metainfo file
    message("appstreamcli not found, metadata may be incomplete.")
    install(FILES ${METAINFO_FILENAME_SRC} DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
endif()
