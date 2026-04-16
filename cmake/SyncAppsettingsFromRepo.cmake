# POST_BUILD: if appsettings.json exists at repo root, copy it next to the executable
# (otherwise the first build may leave the example copy and cwd loads placeholders).
cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED A_SRC OR NOT DEFINED A_DST)
  message(FATAL_ERROR "SyncAppsettingsFromRepo: set -DA_SRC= -DA_DST=")
endif()
if(EXISTS "${A_SRC}")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${A_SRC}" "${A_DST}" RESULT_VARIABLE _r)
  if(NOT _r EQUAL 0)
    message(WARNING "Could not sync ${A_SRC} to ${A_DST}")
  endif()
endif()
