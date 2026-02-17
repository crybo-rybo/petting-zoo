set(PETTING_ZOO_WEB_DIST_DIR "${CMAKE_BINARY_DIR}/web-dist")

add_custom_command(
  OUTPUT "${PETTING_ZOO_WEB_DIST_DIR}/index.html"
  COMMAND ${CMAKE_COMMAND}
    -DWEB_DIR=${CMAKE_CURRENT_SOURCE_DIR}
    -DOUT_DIR=${PETTING_ZOO_WEB_DIST_DIR}
    -DFALLBACK_DIR=${CMAKE_CURRENT_SOURCE_DIR}/fallback-dist
    -DPZ_BUILD_WEB=${PETTING_ZOO_BUILD_WEB}
    -P ${CMAKE_SOURCE_DIR}/cmake/BuildWeb.cmake
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/package.json
    ${CMAKE_CURRENT_SOURCE_DIR}/package-lock.json
    ${CMAKE_CURRENT_SOURCE_DIR}/index.html
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.ts
    ${CMAKE_CURRENT_SOURCE_DIR}/src/App.svelte
    ${CMAKE_CURRENT_SOURCE_DIR}/vite.config.ts
    ${CMAKE_CURRENT_SOURCE_DIR}/fallback-dist/index.html
  COMMENT "Preparing web assets"
  VERBATIM
)

add_custom_target(petting_zoo_web_assets ALL DEPENDS "${PETTING_ZOO_WEB_DIST_DIR}/index.html")
set(PETTING_ZOO_WEB_DIST_DIR "${PETTING_ZOO_WEB_DIST_DIR}" PARENT_SCOPE)
