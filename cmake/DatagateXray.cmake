# Build DataGateXray from third_party/xray-core (XTLS/Xray-core) when Go is available.
# Produces ${CMAKE_BINARY_DIR}/DataGateXray; defines DATAGATE_EMBEDDED_XRAY=1 on the main target.

set(DATAGATE_XRAY_ROOT "${CMAKE_SOURCE_DIR}/third_party/xray-core")
set(DATAGATE_EMBEDDED_XRAY FALSE)
set(DATAGATE_XRAY_BIN "")

if(NOT EXISTS "${DATAGATE_XRAY_ROOT}/main/main.go" AND NOT EXISTS "${DATAGATE_XRAY_ROOT}/go.mod")
    message(STATUS "Xray: third_party/xray-core missing — DataGateXray not built")
    return()
endif()

find_program(DATAGATE_GO_EXECUTABLE go)
if(NOT DATAGATE_GO_EXECUTABLE)
    message(WARNING
        "Xray: Go not found on PATH — DataGateXray not built. "
        "Install Go and reconfigure, or place a prebuilt DataGateXray next to DataGateLinux.")
    return()
endif()

set(DATAGATE_XRAY_OUT "${CMAKE_BINARY_DIR}/DataGateXray")
set(DATAGATE_XRAY_STAMP "${CMAKE_BINARY_DIR}/DataGateXray.stamp")

# Build from module root (main package lives under ./main in current Xray-core layout).
add_custom_command(
    OUTPUT "${DATAGATE_XRAY_OUT}" "${DATAGATE_XRAY_STAMP}"
    COMMAND ${CMAKE_COMMAND} -E env
        "CGO_ENABLED=0"
        ${DATAGATE_GO_EXECUTABLE} build -trimpath -ldflags=-s -o "${DATAGATE_XRAY_OUT}" ./main
    COMMAND ${CMAKE_COMMAND} -E touch "${DATAGATE_XRAY_STAMP}"
    WORKING_DIRECTORY "${DATAGATE_XRAY_ROOT}"
    COMMENT "Building DataGateXray from third_party/xray-core"
    VERBATIM
)

add_custom_target(datagate_xray_bin ALL DEPENDS "${DATAGATE_XRAY_OUT}")

set(DATAGATE_EMBEDDED_XRAY TRUE)
set(DATAGATE_XRAY_BIN "${DATAGATE_XRAY_OUT}")
message(STATUS "Xray: will build DataGateXray with ${DATAGATE_GO_EXECUTABLE}")
