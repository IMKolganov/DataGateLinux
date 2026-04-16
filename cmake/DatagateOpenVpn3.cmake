# Embedded OpenVPN 3 core (same layout as DataGateAndroid/native-openvpn3).
# Expects:
#   third_party/openvpn3/          — OpenVPN 3 core tree (client/, openvpn/, …)
#   third_party/asio/asio/include  — standalone Asio (<asio.hpp>)
#
# Or set before project(): -DDATAGATE_OPENVPN3_ROOT=/path/to/openvpn3 -DDATAGATE_ASIO_INCLUDE_DIR=...

if(NOT DATAGATE_OPENVPN3_ROOT)
    set(DATAGATE_OPENVPN3_ROOT "${CMAKE_SOURCE_DIR}/third_party/openvpn3")
endif()
if(NOT DATAGATE_ASIO_INCLUDE_DIR)
    set(DATAGATE_ASIO_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/asio/asio/include")
endif()

if(NOT EXISTS "${DATAGATE_OPENVPN3_ROOT}/client/ovpncli.hpp")
    message(FATAL_ERROR
        "Embedded OpenVPN 3 sources not found.\n"
        "  Expected: ${DATAGATE_OPENVPN3_ROOT}/client/ovpncli.hpp\n"
        "  Clone https://github.com/OpenVPN/openvpn3 into third_party/openvpn3, and add Asio headers under\n"
        "  third_party/asio/asio/include (or symlink your DataGateAndroid/native-openvpn3/{openvpn3,third_party/asio}).")
endif()
if(NOT EXISTS "${DATAGATE_ASIO_INCLUDE_DIR}/asio.hpp")
    message(FATAL_ERROR
        "Asio headers not found (need <asio.hpp>).\n"
        "  Expected: ${DATAGATE_ASIO_INCLUDE_DIR}/asio.hpp")
endif()

set(O3_DIR "${DATAGATE_OPENVPN3_ROOT}")

file(GLOB_RECURSE _DATAGATE_OVPN3_CORE_SOURCES
    "${O3_DIR}/client/*.cpp"
    "${O3_DIR}/openvpn/*.cpp"
)
list(FILTER _DATAGATE_OVPN3_CORE_SOURCES EXCLUDE REGEX ".*/openvpn/ovpnagent/.*")
list(FILTER _DATAGATE_OVPN3_CORE_SOURCES EXCLUDE REGEX ".*/openvpn/omi/.*")

add_library(datagate_ovpn3_core STATIC ${_DATAGATE_OVPN3_CORE_SOURCES})

set_target_properties(datagate_ovpn3_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LZ4 REQUIRED IMPORTED_TARGET liblz4)

# IV_VER string: same idea as Android (override OPENVPN_VERSION in openvpn/common/version.hpp)
set(_DATAGATE_VER_H "${O3_DIR}/openvpn/common/version.hpp")
if(EXISTS "${_DATAGATE_VER_H}")
    file(READ "${_DATAGATE_VER_H}" _DATAGATE_VER_CONTENT)
    if(_DATAGATE_VER_CONTENT MATCHES "OPENVPN_VERSION \"([^\"]+)\"")
        set(_DATAGATE_OVPN_BASE_VER "${CMAKE_MATCH_1}")
        string(REGEX REPLACE "_git:.*" "" _DATAGATE_OVPN_SEMVER "${_DATAGATE_OVPN_BASE_VER}")
    endif()
endif()
if(NOT _DATAGATE_OVPN_SEMVER)
    set(_DATAGATE_OVPN_SEMVER "3.12")
endif()
set(DATAGATE_OPENVPN_IV_VER "${_DATAGATE_OVPN_SEMVER}_datagate_linux_${PROJECT_VERSION}")

target_include_directories(datagate_ovpn3_core PUBLIC
    ${O3_DIR}
    ${O3_DIR}/client
    ${O3_DIR}/external/fmt/include
    ${DATAGATE_ASIO_INCLUDE_DIR}
)

target_link_libraries(datagate_ovpn3_core PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    PkgConfig::LZ4
    Threads::Threads
)

target_compile_definitions(datagate_ovpn3_core PUBLIC
    FMT_HEADER_ONLY
    ASIO_STANDALONE
    USE_ASIO
    HAVE_LZ4
    USE_OPENSSL
    "OPENVPN_VERSION=\"${DATAGATE_OPENVPN_IV_VER}\""
)

if(UNIX AND NOT APPLE)
    target_compile_definitions(datagate_ovpn3_core PRIVATE OPENVPN_USE_SITNL)
endif()
