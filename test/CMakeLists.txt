add_executable(test_rostatus test_rostatus.cpp)
target_include_directories(test_rostatus PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(test_rostatus PRIVATE rotorrent_base Qt6::Core)
add_test(NAME rostatus COMMAND test_rostatus)

add_executable(test_actionrules test_actionrules.cpp ${PROJECT_SOURCE_DIR}/src/gui/actions/ActionRules.cpp)
target_include_directories(test_actionrules PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(test_actionrules PRIVATE Qt6::Core)
add_test(NAME actionrules COMMAND test_actionrules)

add_executable(smoke_session smoke_session.cpp)
target_include_directories(smoke_session PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(smoke_session PRIVATE rotorrent_app_core Qt6::Core)

if (ROTORRENT_BUILD_GUI AND NOT ROTORRENT_BACKEND_ONLY)
    add_executable(test_chrome test_chrome.cpp)
    target_link_libraries(test_chrome PRIVATE rotorrent_gui)
    add_test(NAME chrome COMMAND test_chrome)

    add_executable(test_transferlist test_transferlist.cpp)
    target_compile_definitions(test_transferlist PRIVATE ROTORRENT_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")
    target_link_libraries(test_transferlist PRIVATE rotorrent_gui)
    add_test(NAME transferlist COMMAND test_transferlist)

    add_executable(test_dragdrop test_dragdrop.cpp)
    target_include_directories(test_dragdrop PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_dragdrop PRIVATE rotorrent_gui)
    add_test(NAME dragdrop COMMAND test_dragdrop)
endif()

if (ROTORRENT_BUILD_GUI AND NOT ROTORRENT_BACKEND_ONLY)
    add_executable(test_commands test_commands.cpp)
    target_include_directories(test_commands PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_commands PRIVATE rotorrent_gui rotorrent_app_core)

    add_executable(test_addtorrent test_addtorrent.cpp)
    target_include_directories(test_addtorrent PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_addtorrent PRIVATE rotorrent_gui rotorrent_app_core)

    add_executable(test_torrentcreator test_torrentcreator.cpp)
    target_include_directories(test_torrentcreator PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_torrentcreator PRIVATE rotorrent_gui rotorrent_app_core)

    add_executable(test_infodialogs test_infodialogs.cpp)
    target_include_directories(test_infodialogs PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_infodialogs PRIVATE rotorrent_gui rotorrent_app_core)

    add_executable(test_options test_options.cpp)
    target_include_directories(test_options PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_options PRIVATE rotorrent_gui rotorrent_app_core)

    add_executable(test_managementdialogs test_managementdialogs.cpp)
    target_include_directories(test_managementdialogs PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_managementdialogs PRIVATE rotorrent_gui rotorrent_app_core)

    # Slice 6: the app-level GUI glue. src/app is built headless (DISABLE_GUI) for the smoke test, so the
    # GUI branches of legalnotice.cpp and crashhandler.cpp are compiled here, against the real dialogs,
    # in a library of their own. The real `rotorrent` executable switches to it in slice 7.
    add_library(rotorrent_app_gui_glue STATIC
        ${PROJECT_SOURCE_DIR}/src/app/legalnotice.cpp
        ${PROJECT_SOURCE_DIR}/src/app/crashhandler.cpp
        ${PROJECT_SOURCE_DIR}/src/app/stacktrace.cpp)
    target_include_directories(rotorrent_app_gui_glue PUBLIC ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/src/app)
    target_link_libraries(rotorrent_app_gui_glue PUBLIC rotorrent_gui rotorrent_base ${CMAKE_DL_LIBS})
    target_link_options(rotorrent_app_gui_glue PUBLIC $<$<PLATFORM_ID:Linux>:-rdynamic>)

    add_executable(test_appdialogs test_appdialogs.cpp)
    target_include_directories(test_appdialogs PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_appdialogs PRIVATE rotorrent_app_gui_glue)

    add_executable(test_details test_details.cpp)
    target_include_directories(test_details PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_details PRIVATE rotorrent_gui rotorrent_app_core)
    add_test(NAME details COMMAND test_details)

    add_executable(test_details_engine test_details_engine.cpp)
    target_include_directories(test_details_engine PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_details_engine PRIVATE rotorrent_gui rotorrent_app_core)

    add_executable(test_adapter test_adapter.cpp)
    target_include_directories(test_adapter PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_adapter PRIVATE rotorrent_gui rotorrent_app_core)
endif()

if (ROTORRENT_BUILD_GUI AND NOT ROTORRENT_BACKEND_ONLY)
    # Phase 8: status bar, tray, toasts, power rule, update check.
    add_executable(test_statusbar test_statusbar.cpp)
    target_include_directories(test_statusbar PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_statusbar PRIVATE rotorrent_gui)
    add_test(NAME statusbar COMMAND test_statusbar)

    add_executable(test_tray test_tray.cpp)
    target_include_directories(test_tray PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_tray PRIVATE rotorrent_gui)
    add_test(NAME tray COMMAND test_tray)

    add_executable(test_power test_power.cpp)
    target_include_directories(test_power PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_power PRIVATE rotorrent_gui)
    add_test(NAME power COMMAND test_power)

    add_executable(test_updatecheck test_updatecheck.cpp)
    target_include_directories(test_updatecheck PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_updatecheck PRIVATE rotorrent_gui)
    add_test(NAME updatecheck COMMAND test_updatecheck)

    add_executable(test_statusfeed test_statusfeed.cpp)
    target_include_directories(test_statusfeed PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_statusfeed PRIVATE rotorrent_gui rotorrent_app_core)
endif()
