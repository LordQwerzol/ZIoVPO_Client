cmake_minimum_required(VERSION 3.19)
project(ClientApp LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

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

)

target_link_libraries(ClientApp
    PRIVATE
        Qt::Core
        Qt::Widgets
)

install(TARGETS ClientApp
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ClientApp
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
