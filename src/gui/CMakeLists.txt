# RoTorrent GUI (Phase 2: theme engine and window chrome).
find_package(Qt6 ${minQt6Version} REQUIRED COMPONENTS Widgets Svg)

file(GLOB_RECURSE ROTORRENT_RO_RESOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../resources/ro/*")

add_library(rotorrent_gui STATIC
    EngineWindow.h
    EngineWindow.cpp
    MainWindow.h
    MainWindow.cpp
    MainWindowCommands.cpp
    actions/ActionRules.h
    actions/ActionRules.cpp
    actions/TorrentController.h
    GuiSettings.h
    GuiSettings.cpp
    options/optionsdialog.h
    options/optionsdialog.cpp
    options/optionsdialog.ui
    options/OptionsCategoryDelegate.h
    options/OptionsCategoryDelegate.cpp
    options/AdvancedPage.h
    options/AdvancedPage.cpp
    options/SubDialogShell.h
    options/OptionsLauncher.h
    options/OptionsLauncher.cpp
    options/fspathedit.h
    options/fspathedit.cpp
    options/fspathedit_p.h
    options/fspathedit_p.cpp
    options/flowlayout.h
    options/flowlayout.cpp
    options/addtorrentparamswidget.h
    options/addtorrentparamswidget.cpp
    options/addtorrentparamswidget.ui
    options/torrentsharelimitswidget.h
    options/torrentsharelimitswidget.cpp
    options/torrentsharelimitswidget.ui
    options/watchedfolderoptionsdialog.h
    options/watchedfolderoptionsdialog.cpp
    options/watchedfolderoptionsdialog.ui
    options/watchedfoldersmodel.h
    options/watchedfoldersmodel.cpp
    options/banlistoptionsdialog.h
    options/banlistoptionsdialog.cpp
    options/banlistoptionsdialog.ui
    actions/TorrentController.cpp
    dialogs/AddLinksDialog.h
    dialogs/AddLinksDialog.cpp
    dialogs/AddNewTorrentDialog.h
    dialogs/AddNewTorrentDialog.cpp
    dialogs/AddTorrentFlow.h
    dialogs/AddTorrentFlow.cpp
    dialogs/TorrentCreatorDialog.h
    dialogs/TorrentCreatorDialog.cpp
    dialogs/TorrentOptionsDialog.h
    dialogs/TorrentOptionsDialog.cpp
    dialogs/ManagementDialogs.h
    dialogs/ManagementDialogs.cpp
    dialogs/InfoDialogs.h
    dialogs/InfoDialogs.cpp
    dialogs/AboutDialog.cpp
    dialogs/ShutdownConfirmDialog.h
    dialogs/ShutdownConfirmDialog.cpp
    dialogs/LegalNoticeDialog.h
    dialogs/LegalNoticeDialog.cpp
    dialogs/StacktraceDialog.h
    dialogs/StacktraceDialog.cpp
    dialogs/RoDialog.h
    dialogs/RoDialog.cpp
    dialogs/RoInputDialog.h
    dialogs/RoInputDialog.cpp
    dialogs/RoMessageBox.h
    dialogs/RoMessageBox.cpp
    widgets/AccentButton.h
    widgets/AccentButton.cpp
    widgets/StatusBar.h
    widgets/StatusBar.cpp
    widgets/NetBanner.h
    widgets/NetBanner.cpp
    tray/RoTray.h
    tray/RoTray.cpp
    tray/Toast.h
    tray/Toast.cpp
    tray/NotificationFeed.h
    tray/NotificationFeed.cpp
    power/PowerController.h
    power/PowerController.cpp
    update/UpdateChecker.h
    update/UpdateChecker.cpp
    StatusFeed.h
    StatusFeed.cpp
    GuiLog.h
    widgets/MainToolbar.h
    widgets/MainToolbar.cpp
    widgets/MenuBuilder.h
    widgets/MenuIcons.h
    widgets/MenuIcons.cpp
    chrome/Hairline.h
    chrome/RoFramelessWindow.h
    chrome/RoFramelessWindow.cpp
    chrome/RoHoverIcon.h
    chrome/RoHoverIcon.cpp
    chrome/winsnap.h
    chrome/winsnap.cpp
    models/DemoSource.h
    models/DemoSource.cpp
    models/SessionAdapter.h
    models/SessionAdapter.cpp
    models/TorrentFilterProxy.h
    models/TorrentFilterProxy.cpp
    models/TorrentRow.h
    models/TransferListModel.h
    models/TransferListModel.cpp
    models/rostatus.h
    widgets/EmptyState.h
    widgets/EmptyState.cpp
    widgets/RoPaint.h
    widgets/RoSearchBox.h
    widgets/RoSearchBox.cpp
    widgets/Sidebar.h
    widgets/Sidebar.cpp
    widgets/TorrentDelegate.h
    widgets/TorrentDelegate.cpp
    widgets/TransferListView.h
    widgets/TransferListView.cpp
    details/DetailsData.h
    details/DetailsSource.h
    details/SessionDetailsSource.h
    details/SessionDetailsSource.cpp
    details/DemoDetailsSource.h
    details/DemoDetailsSource.cpp
    details/LogBuffer.h
    details/LogBuffer.cpp
    details/DetailsWidgets.h
    details/DetailsWidgets.cpp
    details/DetailsDialogs.h
    details/DetailsDialogs.cpp
    details/DetailsTabs.h
    details/GeneralTab.cpp
    details/TableTabs.cpp
    details/ContentTab.cpp
    details/LogTab.cpp
    details/DetailsPanel.h
    details/DetailsPanel.cpp
    theme/Palette.h
    theme/RoTheme.h
    theme/RoTheme.cpp
)

qt_add_resources(rotorrent_gui rotorrent_ro
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/../resources"
    FILES ${ROTORRENT_RO_RESOURCES}
)

target_include_directories(rotorrent_gui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/chrome"
    "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(rotorrent_gui PUBLIC rotorrent_base Qt6::Widgets Qt6::Svg)
target_include_directories(rotorrent_gui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

# Phase 2 development entry point (no torrent engine).
add_executable(rotorrent_window main_window_preview.cpp)
target_link_libraries(rotorrent_window PRIVATE rotorrent_gui rotorrent_app_core)
set_target_properties(rotorrent_window PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
if (WIN32)
    set_target_properties(rotorrent_window PROPERTIES WIN32_EXECUTABLE ON)
    target_sources(rotorrent_window PRIVATE "${CMAKE_BINARY_DIR}/rotorrent.rc")
    if (MSVC)
        target_link_options(rotorrent_window PRIVATE /MANIFEST:NO)
    endif()
endif()
