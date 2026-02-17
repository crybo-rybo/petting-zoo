add_executable(petting_zoo_cpp_sanity
  cpp/config_sanity.cpp
)
target_link_libraries(petting_zoo_cpp_sanity PRIVATE zoo)
target_compile_features(petting_zoo_cpp_sanity PRIVATE cxx_std_20)

add_test(NAME cpp_config_sanity COMMAND petting_zoo_cpp_sanity)

find_program(_curl curl)
if(_curl)
  add_test(
    NAME server_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/integration/server_smoke.sh
            $<TARGET_FILE:petting_zoo_server>
            ${_curl}
  )
else()
  message(WARNING "curl not found; skipping server_smoke test registration")
endif()

find_program(_npm npm)
if(_npm)
  add_test(NAME web_unit_tests COMMAND ${_npm} run test -- --run --reporter=dot)
  set_tests_properties(web_unit_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/apps/web
  )
else()
  message(WARNING "npm not found; skipping web_unit_tests registration")
endif()

add_custom_target(check
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  DEPENDS petting_zoo_cpp_sanity petting_zoo_server petting_zoo_web_assets
  USES_TERMINAL
)
