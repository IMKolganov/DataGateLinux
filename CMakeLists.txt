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
    src/DatagateTr.h
    src/DatagateTranslator.cpp
    src/DatagateTranslator.h
)

target_link_libraries(DataGateLinux PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)

# Regenerate sources: python3 tools/build_catalog_jsonl.py && python3 tools/gen_datagate_ts.py
# (after editing tools/translations_index.py or UI strings). Requires lrelease to ship .qm.
find_program(DATAGATE_LRELEASE NAMES lrelease-qt6 lrelease)
set(DATAGATE_TS_FILES
    ${CMAKE_SOURCE_DIR}/i18n/datagate_en.ts
    ${CMAKE_SOURCE_DIR}/i18n/datagate_ru.ts
    ${CMAKE_SOURCE_DIR}/i18n/datagate_fr.ts
    ${CMAKE_SOURCE_DIR}/i18n/datagate_el.ts
)
if(DATAGATE_LRELEASE)
    set(DATAGATE_QM_FILES "")
    foreach(_ts ${DATAGATE_TS_FILES})
        get_filename_component(_name_we ${_ts} NAME_WE)
        set(_qm ${CMAKE_BINARY_DIR}/i18n/${_name_we}.qm)
        add_custom_command(
            OUTPUT ${_qm}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/i18n
            COMMAND ${DATAGATE_LRELEASE} ${_ts} -qm ${_qm}
            DEPENDS ${_ts}
            COMMENT "lrelease ${_name_we}.qm"
        )
        list(APPEND DATAGATE_QM_FILES ${_qm})
    endforeach()
    add_custom_target(datagate_qm ALL DEPENDS ${DATAGATE_QM_FILES})
    add_dependencies(DataGateLinux datagate_qm)
    add_custom_command(TARGET DataGateLinux POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:DataGateLinux>/i18n
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DATAGATE_QM_FILES} $<TARGET_FILE_DIR:DataGateLinux>/i18n/
        COMMENT "Copy translation .qm next to executable"
    )
else()
    message(WARNING "lrelease not found (install qt6-l10n-tools or qt6-tools); UI will stay in English.")
endif()

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
