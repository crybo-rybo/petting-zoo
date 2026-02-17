if(NOT DEFINED WEB_DIR OR NOT DEFINED OUT_DIR OR NOT DEFINED FALLBACK_DIR)
  message(FATAL_ERROR "BuildWeb.cmake requires WEB_DIR, OUT_DIR, and FALLBACK_DIR")
endif()

set(_built FALSE)

if(PZ_BUILD_WEB)
  find_program(_npm npm)
  if(_npm)
    if(EXISTS "${WEB_DIR}/package-lock.json")
      execute_process(
        COMMAND "${_npm}" ci --no-audit --no-fund
        WORKING_DIRECTORY "${WEB_DIR}"
        RESULT_VARIABLE _npm_ci_result
      )
      if(_npm_ci_result EQUAL 0)
        execute_process(
          COMMAND "${_npm}" run build
          WORKING_DIRECTORY "${WEB_DIR}"
          RESULT_VARIABLE _npm_build_result
        )
        if(_npm_build_result EQUAL 0 AND EXISTS "${WEB_DIR}/dist/index.html")
          set(_built TRUE)
        else()
          message(WARNING "npm run build failed, using fallback web assets")
        endif()
      else()
        message(WARNING "npm ci failed, using fallback web assets")
      endif()
    else()
      message(WARNING "package-lock.json not found, using fallback web assets")
    endif()
  else()
    message(WARNING "npm not found, using fallback web assets")
  endif()
endif()

file(REMOVE_RECURSE "${OUT_DIR}")
file(MAKE_DIRECTORY "${OUT_DIR}")

if(_built)
  file(COPY "${WEB_DIR}/dist/" DESTINATION "${OUT_DIR}")
else()
  file(COPY "${FALLBACK_DIR}/" DESTINATION "${OUT_DIR}")
endif()
