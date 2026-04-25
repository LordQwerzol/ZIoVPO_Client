add_executable(ZIoVPOService
    src/WinService.cpp
    src/RpcServer.cpp
    src/UiClientManager.cpp
    src/Utils.cpp
    src/AuthManager.cpp
    src/LicenseManager.cpp
    src/HttpClient.cpp
    src/main.cpp
)

include(FetchContent)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/include.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

target_include_directories(ZIoVPOService PRIVATE ${json_SOURCE_DIR}/include)
target_include_directories(ZIoVPOService PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(ZIoVPOService PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")

target_link_libraries(ZIoVPOService
    CommonRpc
    Rpcrt4
    Wtsapi32
    Advapi32
    Userenv
    winhttp
    iphlpapi
)

install(TARGETS ZIoVPOService
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    CONFIGURATIONS Release
)