cmake_minimum_required(VERSION 3.16)
project(DataGateLinux LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebSockets)

qt_add_executable(DataGateLinux
    src/main.cpp
    src/AppLogging.cpp
    src/AppLogging.h
    src/AppConfig.cpp
    src/AppConfig.h
    src/AuthSession.cpp
    src/AuthSession.h
    src/LoginDialog.cpp
    src/LoginDialog.h
    src/AppTheme.cpp
    src/AppTheme.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/StatisticsPanel.cpp
    src/StatisticsPanel.h
    src/VpnSession.cpp
    src/VpnSession.h
    src/WssTcpBridge.cpp
    src/WssTcpBridge.h
    src/UdpWssBridge.cpp
    src/UdpWssBridge.h
    src/GoogleAuthHelper.cpp
    src/GoogleAuthHelper.h
    src/DatagateUtils.cpp
    src/DatagateUtils.h
)

target_link_libraries(DataGateLinux PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)

if(UNIX AND NOT APPLE)
    target_compile_definitions(DataGateLinux PRIVATE DATAGATE_LINUX=1)
endif()

# Verbose stderr logging for IDE: Debug / RelWithDebInfo, or DATAGATE_LOG=1 at runtime
target_compile_definitions(DataGateLinux PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:DATAGATE_DEBUG_LOG=1>
)

# Copy example template if appsettings.json is missing next to the exe;
# if appsettings.json exists at repo root, sync it beside the exe (current BaseUrl / ClientId).
add_custom_command(TARGET DataGateLinux POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/appsettings.example.json"
    "$<TARGET_FILE_DIR:DataGateLinux>/appsettings.example.json"
  COMMAND ${CMAKE_COMMAND}
    -DA_SOURCE="${CMAKE_SOURCE_DIR}/appsettings.example.json"
    -DA_DST=$<TARGET_FILE_DIR:DataGateLinux>/appsettings.json
    -P "${CMAKE_SOURCE_DIR}/cmake/CopyAppsettingsIfMissing.cmake"
  COMMAND ${CMAKE_COMMAND}
    -DA_SRC="${CMAKE_SOURCE_DIR}/appsettings.json"
    -DA_DST=$<TARGET_FILE_DIR:DataGateLinux>/appsettings.json
    -P "${CMAKE_SOURCE_DIR}/cmake/SyncAppsettingsFromRepo.cmake"
  COMMENT "Bundle appsettings; seed from example if missing; sync from repo root if present"
)

install(TARGETS DataGateLinux RUNTIME DESTINATION bin)
