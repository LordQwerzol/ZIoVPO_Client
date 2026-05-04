cmake_minimum_required(VERSION 3.19)
project(ClientApp LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

set(APP_ICON_RC "${CMAKE_CURRENT_SOURCE_DIR}/resources/app_icon.rc")

qt_add_resources(APP_RESOURCES resources.qrc)

qt_add_executable(ClientApp
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    ${APP_RESOURCES}
    traymanager.h traymanager.cpp
    TaskbarCreatedFilter.h
    ServiceClient.h ServiceClient.cpp
    ${APP_ICON_RC}
    authdialog.h authdialog.cpp authdialog.ui
)

set(APP_ICON_RC "${APP_RESOURCES}/app_icon.rc")

target_link_libraries(ClientApp
    PRIVATE
        Qt::Core
        Qt::Widgets
        CommonRpc
        Rpcrt4
        advapi32
        wtsapi32
        user32
        advapi32
)

install(TARGETS ClientApp
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ClientApp
    OUTPUT_SCRIPT deploy_script
    DEPLOY_TOOL_OPTIONS --no-quick-import
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
