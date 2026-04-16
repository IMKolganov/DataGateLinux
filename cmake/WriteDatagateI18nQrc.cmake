# Build-time: write an RCC file that embeds *.qm from QMDIR (absolute paths in <file>).
# Usage: cmake -DQMDIR=/path/to/dir/with/qm -DOUT=/path/to/datagate_i18n_embed.qrc -P WriteDatagateI18nQrc.cmake
if(NOT QMDIR OR NOT OUT)
  message(FATAL_ERROR "WriteDatagateI18nQrc.cmake: set QMDIR and OUT")
endif()
file(GLOB _qms "${QMDIR}/*.qm")
file(WRITE "${OUT}" "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<RCC>\n  <qresource prefix=\"/i18n\">\n")
foreach(_qm ${_qms})
  get_filename_component(_name "${_qm}" NAME)
  file(APPEND "${OUT}" "    <file alias=\"${_name}\">${_qm}</file>\n")
endforeach()
file(APPEND "${OUT}" "  </qresource>\n</RCC>\n")
