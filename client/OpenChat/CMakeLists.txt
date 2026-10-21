cmake_minimum_required(VERSION 3.16)

project(OpenChat VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(appOpenChat

)

qt_add_qml_module(appOpenChat
    URI OpenChat
    VERSION 1.0
    QML_FILES
        Main.qml
        QML_FILES LoginPage.qml
        QML_FILES RegisterPage.qml
        SOURCES main.cpp
        SOURCES singleton.h
        SOURCES httpmgr.h httpmgr.cpp
        SOURCES global.h
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appOpenChat PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appOpenChat
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appOpenChat
    PRIVATE Qt6::Quick
)

# 构建后把 config.ini 放到可执行文件同目录，供程序启动时读取
add_custom_command(TARGET appOpenChat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config.ini"
        "$<TARGET_FILE_DIR:appOpenChat>/config.ini"
)

include(GNUInstallDirs)
install(TARGETS appOpenChat
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
