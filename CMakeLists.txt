cmake_minimum_required(VERSION 3.16)
project(ddplugin-videowallpaper VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui DBus Concurrent Network)
find_package(Dtk6 REQUIRED COMPONENTS Core)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil libswscale)

# 手动链接 dfm6，避免 find_package 强依赖未装全的 dtk 组件
find_library(DFM6_BASE_LIB dfm6-base REQUIRED)
find_library(DFM6_FRAMEWORK_LIB dfm6-framework REQUIRED)

set(PLUGIN_SOURCES
    src/videowallpaperplugin.cpp
    src/wallpaperengine.cpp
    src/wallpaperconfig.cpp
    src/videoproxy.cpp
    src/videodecoder.cpp
    src/videowallpapermenuscene.cpp
    src/settingsdialog.cpp
)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES} src/videowallpaper.json)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "dd-videowallpaper-plugin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    PREFIX "lib"
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::DBus
    Qt6::Concurrent
    Qt6::Network
    Dtk6::Core
    ${DFM6_BASE_LIB}
    ${DFM6_FRAMEWORK_LIB}
    ${FFMPEG_LIBRARIES}
)

target_link_options(${PROJECT_NAME} PRIVATE -Wl,--no-undefined)

# Deepin 桌面 edge 插件目录（官方约定）
if(NOT DEFINED PLUGIN_INSTALL_DIR)
    set(PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/dde-file-manager/plugins/desktop-edge")
endif()

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR}
    NAMELINK_SKIP
)

# 文档
install(FILES README.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    OPTIONAL
)
