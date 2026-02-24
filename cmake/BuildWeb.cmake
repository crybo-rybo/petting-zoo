if(NOT DEFINED WEB_DIR OR NOT DEFINED OUT_DIR OR NOT DEFINED FALLBACK_DIR OR NOT DEFINED PZ_ALLOW_FALLBACK)
  message(FATAL_ERROR "BuildWeb.cmake requires WEB_DIR, OUT_DIR, FALLBACK_DIR, and PZ_ALLOW_FALLBACK")
endif()

set(_built FALSE)
set(_fallback_reason "")
set(_use_fallback FALSE)

function(_handle_web_build_failure reason)
  if(PZ_ALLOW_FALLBACK)
    set(_use_fallback TRUE PARENT_SCOPE)
    set(_fallback_reason "${reason}" PARENT_SCOPE)
  else()
    message(FATAL_ERROR "${reason}")
  endif()
endfunction()

if(PZ_BUILD_WEB)
  find_program(_npm npm)
  if(_npm)
    if(EXISTS "${WEB_DIR}/package-lock.json")
      set(_npm_ci_stamp "${WEB_DIR}/node_modules/.petting-zoo-npm-ci.stamp")
      set(_needs_npm_ci TRUE)
      if(EXISTS "${WEB_DIR}/node_modules" AND EXISTS "${_npm_ci_stamp}")
        if(NOT "${WEB_DIR}/package-lock.json" IS_NEWER_THAN "${_npm_ci_stamp}" AND
           NOT "${WEB_DIR}/package.json" IS_NEWER_THAN "${_npm_ci_stamp}")
          set(_needs_npm_ci FALSE)
        endif()
      endif()

      if(_needs_npm_ci)
        execute_process(
          COMMAND "${_npm}" ci --no-audit --no-fund
          WORKING_DIRECTORY "${WEB_DIR}"
          RESULT_VARIABLE _npm_ci_result
        )
        if(_npm_ci_result EQUAL 0)
          file(WRITE "${_npm_ci_stamp}" "ok\n")
        else()
          _handle_web_build_failure("npm ci failed in ${WEB_DIR}")
        endif()
      endif()

      if(NOT _use_fallback)
        execute_process(
          COMMAND "${_npm}" run build -- --outDir "${OUT_DIR}" --emptyOutDir
          WORKING_DIRECTORY "${WEB_DIR}"
          RESULT_VARIABLE _npm_build_result
        )
        if(_npm_build_result EQUAL 0 AND EXISTS "${OUT_DIR}/index.html")
          set(_built TRUE)
        else()
          _handle_web_build_failure("npm run build failed in ${WEB_DIR}")
        endif()
      endif()
    else()
      _handle_web_build_failure("package-lock.json not found in ${WEB_DIR}")
    endif()
  else()
    _handle_web_build_failure("npm not found in PATH")
  endif()
else()
  set(_use_fallback TRUE)
  set(_fallback_reason "PETTING_ZOO_BUILD_WEB is OFF")
endif()

if(_built)
  message(STATUS "Web assets built at ${OUT_DIR}")
elseif(_use_fallback)
  file(REMOVE_RECURSE "${OUT_DIR}")
  file(MAKE_DIRECTORY "${OUT_DIR}")
  file(COPY "${FALLBACK_DIR}/" DESTINATION "${OUT_DIR}")
  message(WARNING "Using fallback web assets at ${OUT_DIR} (${_fallback_reason})")
else()
  message(FATAL_ERROR "Web assets were not built and fallback is unavailable")
endif()
