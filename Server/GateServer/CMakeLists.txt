cmake_minimum_required(VERSION 4.2.3)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
# cmake_minimum_required(VERSION 4.3.0)
# set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
set(CMAKE_CXX_MODULE_STD ON)
function(_add_executable target)
    add_executable(${target})
endfunction()

project(GateServer VERSION 0.1 LANGUAGES CXX)

_add_executable(GateServer)

target_compile_features(GateServer PRIVATE cxx_std_23)

target_sources(GateServer
    PRIVATE
        main.cpp
        cserver.h
        cserver.cpp
        const.h
        httpconnection.h httpconnection.cpp
        Singleton.h
        logicsystem.h logicsystem.cpp
        ConfigMgr.h ConfigMgr.cpp
        VerifyClient.h VerifyClient.cpp
)

# Boost (asio / beast 为 header-only)
find_package(Boost REQUIRED)
# jsoncpp 提供 Json::Value / Json::CharReaderBuilder 的实现，需要链接
target_link_libraries(GateServer PRIVATE Boost::boost jsoncpp)

add_custom_command(TARGET GateServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config.ini"
        "$<TARGET_FILE_DIR:GateServer>/config.ini"
)

# Deployment settings
include(GNUInstallDirs)
set(CMAKE_INSTALL_PREFIX "/opt/GateServer" CACHE PATH "Install path prefix" FORCE)
install(TARGETS GateServer
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)























































#https://github.com/Kitware/CMake/blob/v4.2.3/Help/dev/experimental.rst
#####  下面的配置，让GateServer链接部署到/opt/lib下面的libutils  #####
#target_include_directories(GateServer PRIVATE /opt/include)  # -I /opt/utils/include 自定义头文件搜索路径
# target_link_directories(GateServer PRIVATE /opt/utils/lib)  # -L /opt/utils/lib 设置链接路径
# target_link_libraries(GateServer PRIVATE utils)             # -lutils           设置链接库名

# set_target_properties(GateServer PROPERTIES
#     INSTALL_RPATH_USE_LINK_PATH  True                           # 使用链接路径作为安装文件的RPath，等价于"-Wl,-rpath=/opt/utils/lib"
# )

#add_library(GateServer SHARED
#  utils.cpp
#)

#set_target_properties( GateServer PROPERTIES
#    PUBLIC_HEADER utils.cppm
#    VERSION   2.5.1
#    SOVERSION 2
#    LIBRARY_OUTPUT_DIRECTORY  "${CMAKE_BINARY_DIR}/lib"
#)

#install(TARGETS GateServer
#    LIBRARY DESTINATION /opt/utils/lib
#    PUBLIC_HEADER DESTINATION  /opt/utils/modules
#)

#add_custom_command(TARGET GateServer
#    POST_BUILD
#    COMMAND cmake --install "${CMAKE_CURRENT_BINARY_DIR}"
#)

# set(MODULE_LIB_DIR /opt/utils/)  # 模块库目录的设置
# set(GATESERVER_MODULE_FILES
#     ${MODULE_LIB_DIR}/utils.cppm          # 模块库的模块文件
#     math.cppm                             # 模块文件
# )
# source_group("GateServer" FILES ${GATESERVER_MODULE_FILES})
#
# target_sources(GateServer
#     PRIVATE FILE_SET cxx_modules TYPE CXX_MODULES
#         BASE_DIRS   # 配置BASE_DIRS，用于组织构建输出目录，以便构建不同目录中(同名的)模块文件.
#             ${MODULE_LIB_DIR}
#             ${CMAKE_CURRENT_SOURCE_DIR}
#         FILES
#             ${GATESERVER_MODULE_FILES}
# )
