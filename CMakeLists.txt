cmake_minimum_required(VERSION 3.16)
project(DataGateLinux VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/openvpn3/client/ovpncli.hpp"
   AND EXISTS "${CMAKE_SOURCE_DIR}/third_party/asio/asio/include/asio.hpp")
    option(DATAGATE_USE_EMBEDDED_OPENVPN3 "Link embedded OpenVPN 3 core (third_party/openvpn3 + asio)" ON)
else()
    option(DATAGATE_USE_EMBEDDED_OPENVPN3 "Link embedded OpenVPN 3 core (third_party/openvpn3 + asio)" OFF)
endif()

if(DATAGATE_USE_EMBEDDED_OPENVPN3)
    include("${CMAKE_SOURCE_DIR}/cmake/DatagateOpenVpn3.cmake")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebSockets)

set(DATAGATE_OVPN3_EMBED_LINUX_HELPER FALSE)
set(DATAGATE_OVPN3_SOURCES "")
if(DATAGATE_USE_EMBEDDED_OPENVPN3)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(DATAGATE_OVPN3_EMBED_LINUX_HELPER TRUE)
        add_executable(DataGateOvpn3Helper
            src/DataGateOvpn3HelperMain.cpp
            src/DatagateOvpn3Client.cpp
            src/LinuxCapNetAdmin.cpp
        )
        set_target_properties(DataGateOvpn3Helper PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
        )
        target_link_libraries(DataGateOvpn3Helper PRIVATE datagate_ovpn3_core)
    else()
        list(APPEND DATAGATE_OVPN3_SOURCES src/DatagateOvpn3Client.cpp)
    endif()
endif()

qt_add_executable(DataGateLinux
    src/main.cpp
    src/AppLogging.cpp
    src/AppLogging.h
    src/AppConfig.cpp
    src/AppConfig.h
    src/GitHubReleaseChecker.cpp
    src/GitHubReleaseChecker.h
    src/AuthSession.cpp
    src/AuthSession.h
    src/DatagateAuthLogin.cpp
    src/DatagateAuthLogin.h
    src/AdminTotpSetupDialog.cpp
    src/AdminTotpSetupDialog.h
    src/FreeTierOnboardingDialog.cpp
    src/FreeTierOnboardingDialog.h
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
    ${DATAGATE_OVPN3_SOURCES}
    src/WssTcpBridge.cpp
    src/WssTcpBridge.h
    src/UdpWssBridge.cpp
    src/UdpWssBridge.h
    src/GoogleAuthHelper.cpp
    src/GoogleAuthHelper.h
    src/DatagateUtils.cpp
    src/DatagateUtils.h
    src/DatagateTr.h
    src/DatagateTr.cpp
    src/DatagateTranslator.cpp
    src/DatagateTranslator.h
    ${CMAKE_SOURCE_DIR}/i18n/datagate_ts_embed.qrc
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(DataGateLinux PRIVATE src/LinuxCapNetAdmin.cpp)
endif()

target_link_libraries(DataGateLinux PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)

if(DATAGATE_USE_EMBEDDED_OPENVPN3)
    target_compile_definitions(DataGateLinux PRIVATE DATAGATE_EMBEDDED_OPENVPN3=1)
    if(DATAGATE_OVPN3_EMBED_LINUX_HELPER)
        target_compile_definitions(DataGateLinux PRIVATE DATAGATE_EMBEDDED_OPENVPN3_USE_EXTERNAL_HELPER=1)
        add_dependencies(DataGateLinux DataGateOvpn3Helper)
        add_custom_command(TARGET DataGateLinux POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:DataGateOvpn3Helper>
                $<TARGET_FILE_DIR:DataGateLinux>
            COMMENT "Copy DataGateOvpn3Helper next to DataGateLinux"
        )
    else()
        target_link_libraries(DataGateLinux PRIVATE datagate_ovpn3_core)
    endif()
endif()

# libcap: raise permitted→effective CAP_NET_ADMIN in-process (non-Linux helper embedded path only).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_library(DATAGATE_LIBCAP cap)
    if(DATAGATE_LIBCAP)
        if(DATAGATE_USE_EMBEDDED_OPENVPN3 AND DATAGATE_OVPN3_EMBED_LINUX_HELPER)
            target_compile_definitions(DataGateOvpn3Helper PRIVATE DATAGATE_HAVE_LIBCAP=1)
            target_link_libraries(DataGateOvpn3Helper PRIVATE ${DATAGATE_LIBCAP})
            message(STATUS "libcap linked to DataGateOvpn3Helper: ${DATAGATE_LIBCAP}")
        elseif(DATAGATE_USE_EMBEDDED_OPENVPN3)
            target_compile_definitions(DataGateLinux PRIVATE DATAGATE_HAVE_LIBCAP=1)
            target_link_libraries(DataGateLinux PRIVATE ${DATAGATE_LIBCAP})
            message(STATUS "libcap linked to DataGateLinux: ${DATAGATE_LIBCAP} (in-process embedded OpenVPN)")
        endif()
    endif()
endif()

# Regenerate sources: python3 tools/build_catalog_jsonl.py && python3 tools/gen_datagate_ts.py
# lrelease: often NOT on PATH when building from an IDE; search next to this Qt6 and usual distro paths.
set(DATAGATE_TS_FILES
    ${CMAKE_SOURCE_DIR}/i18n/datagate_en.ts
    ${CMAKE_SOURCE_DIR}/i18n/datagate_ru.ts
    ${CMAKE_SOURCE_DIR}/i18n/datagate_fr.ts
    ${CMAKE_SOURCE_DIR}/i18n/datagate_el.ts
)

find_program(DATAGATE_LRELEASE NAMES lrelease-qt6 lrelease
    HINTS
        "${Qt6_DIR}/../../../bin"
        "${Qt6_DIR}/../../../libexec"
        "${Qt6_DIR}/../../../../bin"
        /usr/lib/qt6/bin
        /usr/lib/x86_64-linux-gnu/qt6/bin
)

set(DATAGATE_QM_FILES "")
set(DATAGATE_QM_DIR "${CMAKE_BINARY_DIR}/i18n")

if(DATAGATE_LRELEASE)
    foreach(_ts ${DATAGATE_TS_FILES})
        get_filename_component(_name_we ${_ts} NAME_WE)
        set(_qm ${DATAGATE_QM_DIR}/${_name_we}.qm)
        add_custom_command(
            OUTPUT ${_qm}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${DATAGATE_QM_DIR}
            COMMAND ${DATAGATE_LRELEASE} ${_ts} -qm ${_qm}
            DEPENDS ${_ts}
            COMMENT "lrelease ${_name_we}.qm"
        )
        list(APPEND DATAGATE_QM_FILES ${_qm})
    endforeach()
    add_custom_target(datagate_qm ALL DEPENDS ${DATAGATE_QM_FILES})
    add_dependencies(DataGateLinux datagate_qm)
else()
    file(GLOB DATAGATE_QM_FILES "${CMAKE_SOURCE_DIR}/i18n/datagate_*.qm")
    if(DATAGATE_QM_FILES)
        message(STATUS "Using prebuilt i18n/datagate_*.qm (run lrelease after editing .ts when tools are installed).")
        set(DATAGATE_QM_DIR "${CMAKE_SOURCE_DIR}/i18n")
    endif()
endif()

if(DATAGATE_QM_FILES)
    set(DATAGATE_I18N_EMBED_QRC "${CMAKE_BINARY_DIR}/datagate_i18n_embed.qrc")
    add_custom_command(
        OUTPUT "${DATAGATE_I18N_EMBED_QRC}"
        COMMAND ${CMAKE_COMMAND}
            -DQMDIR=${DATAGATE_QM_DIR}
            -DOUT=${DATAGATE_I18N_EMBED_QRC}
            -P ${CMAKE_SOURCE_DIR}/cmake/WriteDatagateI18nQrc.cmake
        DEPENDS ${DATAGATE_QM_FILES}
        COMMENT "Generate datagate_i18n_embed.qrc (embedded .qm)"
    )
    target_sources(DataGateLinux PRIVATE "${DATAGATE_I18N_EMBED_QRC}")
    set_source_files_properties("${DATAGATE_I18N_EMBED_QRC}" PROPERTIES GENERATED ON)

    add_custom_command(TARGET DataGateLinux POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:DataGateLinux>/i18n
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DATAGATE_QM_FILES} $<TARGET_FILE_DIR:DataGateLinux>/i18n/
        COMMENT "Copy translation .qm next to executable"
    )
else()
    message(WARNING
        "No translation .qm: install qt6-l10n-tools (provides lrelease under /usr/lib/qt6/bin), "
        "then delete CMakeCache and reconfigure, or commit prebuilt i18n/datagate_*.qm.")
endif()

if(UNIX AND NOT APPLE)
    target_compile_definitions(DataGateLinux PRIVATE DATAGATE_LINUX=1)
endif()

# Shown in UI / API / OpenVPN peer-info (UV_DATAGATE_APP); bump when releasing.
target_compile_definitions(DataGateLinux PRIVATE "DATAGATE_APP_VERSION_STR=\"${PROJECT_VERSION}\"")

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
if(TARGET DataGateOvpn3Helper)
    install(TARGETS DataGateOvpn3Helper RUNTIME DESTINATION bin)
endif()
