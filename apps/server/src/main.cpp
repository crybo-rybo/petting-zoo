#include <drogon/drogon.h>

#include <filesystem>

#include "routes.hpp"
#include "runtime_state.hpp"

int main() {
  namespace fs = std::filesystem;

  static RuntimeState runtime_state;

  const fs::path web_root = fs::path(PETTING_ZOO_WEB_ROOT);
  const fs::path index_html = web_root / "index.html";

  drogon::app().setLogLevel(trantor::Logger::kWarn);
  drogon::app().setDocumentRoot(web_root.string());

  register_health_routes();
  register_model_routes(runtime_state);
  register_chat_routes(runtime_state);
  register_mcp_routes(runtime_state);
  register_deferred_routes();
  register_spa_routes(web_root, index_html);

  drogon::app().addListener("127.0.0.1", 8080);
  drogon::app().run();
  return 0;
}
