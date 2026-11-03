include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(base)

if (NOT ROTORRENT_BACKEND_ONLY)
    add_subdirectory(app)
    if (ROTORRENT_BUILD_GUI)
        add_subdirectory(gui)
    endif()
endif()
