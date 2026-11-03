# RoTorrent application shell (handoff section 7).
# Phase 1: headless build (DISABLE_GUI) used by the smoke test.
# Phase 2+: the RoTorrent GUI replaces the DISABLE_GUI branches.
add_library(rotorrent_app_core STATIC
    application.h
    applicationinstancemanager.h
    cmdoptions.h
    filelogger.h
    firstrun.h
    legalnotice.h
    localpeer.h

    application.cpp
    applicationinstancemanager.cpp
    cmdoptions.cpp
    filelogger.cpp
    firstrun.cpp
    legalnotice.cpp
    localpeer.cpp
)

target_compile_definitions(rotorrent_app_core PUBLIC DISABLE_GUI DISABLE_WEBUI)
target_link_libraries(rotorrent_app_core PUBLIC rotorrent_base)

# Stack trace support (handoff 5.3, ROTORRENT_STACKTRACE): boost::stacktrace capture + crash handlers.
# The Crash info dialog is shown by the GUI glue (below); the headless core only writes the text to stderr.
if (ROTORRENT_STACKTRACE)
    target_compile_definitions(rotorrent_app_core PUBLIC STACKTRACE)
    target_sources(rotorrent_app_core PRIVATE stacktrace.h stacktrace.cpp crashhandler.h crashhandler.cpp)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(rotorrent_app_core PUBLIC ${CMAKE_DL_LIBS})
        target_link_options(rotorrent_app_core PUBLIC -rdynamic)
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        if (MSVC)
            # boost::stacktrace reads the PDB next to the executable
            target_compile_options(rotorrent_app_core PRIVATE /Zi)
            target_link_options(rotorrent_app_core PUBLIC /DEBUG /PDBALTPATH:$<TARGET_PDB_FILE_NAME:rotorrent_app>)
        else()
            target_link_options(rotorrent_app_core PUBLIC LINKER:--export-dynamic)
        endif()
    endif()
endif()

# The headless executable (no Qt Widgets): used by the smoke test and by CI on hosts without a GUI. When the
# GUI is built, `rotorrent` is the GUI executable below and this one is called rotorrent_headless.
if (ROTORRENT_BUILD_GUI)
    set(ROTORRENT_HEADLESS_NAME rotorrent_headless)
else()
    set(ROTORRENT_HEADLESS_NAME rotorrent)
endif()
add_executable(rotorrent_app main.cpp)
target_link_libraries(rotorrent_app PRIVATE rotorrent_app_core)
set_target_properties(rotorrent_app PROPERTIES
    OUTPUT_NAME ${ROTORRENT_HEADLESS_NAME}
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Phase 6 slice 7: the real RoTorrent executable. The same application sources, compiled without DISABLE_GUI,
# so Application is a QApplication that opens the RoTorrent main window once the session has restored its
# torrents (see Application::exec), and command-line adds, second-instance forwarding, the legal notice, the
# shutdown confirmation and the crash dialog all use the styled dialogs.
if (ROTORRENT_BUILD_GUI)
    add_executable(rotorrent_gui_app
        application.h
        applicationinstancemanager.h
        cmdoptions.h
        filelogger.h
        firstrun.h
        legalnotice.h
        localpeer.h

        application.cpp
        applicationinstancemanager.cpp
        cmdoptions.cpp
        filelogger.cpp
        firstrun.cpp
        legalnotice.cpp
        localpeer.cpp
        main.cpp
    )
    target_compile_definitions(rotorrent_gui_app PRIVATE DISABLE_WEBUI)
    target_include_directories(rotorrent_gui_app PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(rotorrent_gui_app PRIVATE rotorrent_gui rotorrent_base)
    set_target_properties(rotorrent_gui_app PROPERTIES
        OUTPUT_NAME rotorrent
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    if (ROTORRENT_STACKTRACE)
        target_compile_definitions(rotorrent_gui_app PRIVATE STACKTRACE)
        target_sources(rotorrent_gui_app PRIVATE stacktrace.h stacktrace.cpp crashhandler.h crashhandler.cpp)
        if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_link_libraries(rotorrent_gui_app PRIVATE ${CMAKE_DL_LIBS})
            target_link_options(rotorrent_gui_app PRIVATE -rdynamic)
        elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
            if (MSVC)
                target_compile_options(rotorrent_gui_app PRIVATE /Zi)
                target_link_options(rotorrent_gui_app PRIVATE /DEBUG /PDBALTPATH:$<TARGET_PDB_FILE_NAME:rotorrent_gui_app>)
            else()
                target_link_options(rotorrent_gui_app PRIVATE LINKER:--export-dynamic)
            endif()
        endif()
    endif()
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set_target_properties(rotorrent_gui_app PROPERTIES WIN32_EXECUTABLE ON)
        # Icon, version block and manifest (PerMonitorV2 DPI): configured in the top-level CMakeLists.txt.
        target_sources(rotorrent_gui_app PRIVATE "${CMAKE_BINARY_DIR}/rotorrent.rc")
        # The .rc already embeds our manifest; stop the MSVC linker adding a second one (CVT1100).
        if (MSVC)
            target_link_options(rotorrent_gui_app PRIVATE /MANIFEST:NO)
        endif()
    endif()
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set_target_properties(rotorrent_app PROPERTIES WIN32_EXECUTABLE ON)
endif()
