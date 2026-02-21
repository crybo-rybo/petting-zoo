add_executable(petting_zoo_server
  src/api_parsers.cpp
  src/api_serialization.cpp
  src/http_helpers.cpp
  src/routes_chat.cpp
  src/routes_deferred.cpp
  src/routes_health.cpp
  src/routes_models.cpp
  src/routes_spa.cpp
  src/runtime_state.cpp
  src/main.cpp
)

if(TARGET drogon)
  target_link_libraries(petting_zoo_server PRIVATE drogon)
else()
  target_link_libraries(petting_zoo_server PRIVATE Drogon::Drogon)
endif()

target_link_libraries(petting_zoo_server PRIVATE zoo)
target_link_libraries(petting_zoo_server PRIVATE zoo_backend)

target_compile_features(petting_zoo_server PRIVATE cxx_std_20)

target_compile_definitions(petting_zoo_server PRIVATE
  PETTING_ZOO_VERSION="${PROJECT_VERSION}"
  PETTING_ZOO_WEB_ROOT="${PETTING_ZOO_WEB_DIST_DIR}"
)

if(MSVC)
  target_compile_options(petting_zoo_server PRIVATE /W4)
else()
  target_compile_options(petting_zoo_server PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_dependencies(petting_zoo_server petting_zoo_web_assets)
