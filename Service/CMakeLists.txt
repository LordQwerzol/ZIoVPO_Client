add_executable(ZIoVPOService
    src/WinService.cpp
    src/RpcServer.cpp
    src/UiClientManager.cpp
    src/main.cpp
)

target_include_directories(ZIoVPOService PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(ZIoVPOService PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")

target_link_libraries(ZIoVPOService
    CommonRpc
    Rpcrt4
    Wtsapi32
    Advapi32
    Userenv
)

install(TARGETS ZIoVPOService
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    CONFIGURATIONS Release
)