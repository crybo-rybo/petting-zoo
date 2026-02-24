#include <drogon/drogon.h>
#include <json/json.h>
#include <fstream>
#include <filesystem>
#include <string>

#include "routes.hpp"
#include "runtime_state.hpp"

RuntimeConfig load_config(const std::string& config_path, int& port, std::string& host, trantor::Logger::LogLevel& log_level) {
  RuntimeConfig config;
  std::ifstream file(config_path);
  if (!file.is_open()) {
    if (const char* env_port = std::getenv("PORT")) {
      try { port = std::stoi(env_port); } catch (...) {}
    }
    return config;
  }

  Json::Value root;
  Json::Reader reader;
  if (!reader.parse(file, root, false)) {
    LOG_ERROR << "Failed to parse " << config_path;
    if (const char* env_port = std::getenv("PORT")) {
      try { port = std::stoi(env_port); } catch (...) {}
    }
    return config;
  }

  if (root.isMember("server")) {
    const auto& server = root["server"];
    if (server.isMember("host") && server["host"].isString()) host = server["host"].asString();
    if (server.isMember("port") && server["port"].isInt()) port = server["port"].asInt();
  }
  
  if (const char* env_port = std::getenv("PORT")) {
    try {
      port = std::stoi(env_port);
    } catch (...) {
      // Ignore invalid PORT env var
    }
  }
  
  if (root.isMember("runtime")) {
    const auto& runtime = root["runtime"];
    if (runtime.isMember("model_discovery_paths") && runtime["model_discovery_paths"].isArray()) {
      config.model_discovery_paths.clear();
      for (const auto& path : runtime["model_discovery_paths"]) {
        config.model_discovery_paths.push_back(path.asString());
      }
    }
  }

  if (root.isMember("observability") && root["observability"].isMember("log_level")) {
    std::string level = root["observability"]["log_level"].asString();
    if (level == "trace") log_level = trantor::Logger::kTrace;
    else if (level == "debug") log_level = trantor::Logger::kDebug;
    else if (level == "info") log_level = trantor::Logger::kInfo;
    else if (level == "warn") log_level = trantor::Logger::kWarn;
    else if (level == "error") log_level = trantor::Logger::kError;
    else if (level == "fatal") log_level = trantor::Logger::kFatal;
  }

#ifdef ZOO_ENABLE_MCP
  if (root.isMember("mcp_connectors") && root["mcp_connectors"].isArray()) {
    for (const auto& conn : root["mcp_connectors"]) {
      McpConnectorEntry entry;
      entry.id = conn["id"].asString();
      entry.config.server_id = entry.id;
      entry.config.transport.command = conn["command"].asString();
      if (conn.isMember("args") && conn["args"].isArray()) {
        for (const auto& arg : conn["args"]) {
          entry.config.transport.args.push_back(arg.asString());
        }
      }
      config.mcp_connectors.push_back(entry);
    }
  }
#endif

  if (root.isMember("server") && root["server"].isMember("allowed_origins") && root["server"]["allowed_origins"].isArray()) {
    config.allowed_origins.clear();
    for (const auto& origin : root["server"]["allowed_origins"]) {
      config.allowed_origins.push_back(origin.asString());
    }
  }

  return config;
}

int main() {
  namespace fs = std::filesystem;

  int port = 8080;
  std::string host = "127.0.0.1";
  auto log_level = trantor::Logger::kWarn;

  RuntimeConfig config = load_config("config/app.json", port, host, log_level);

  static RuntimeState runtime_state(config);

  const fs::path web_root = fs::path(PETTING_ZOO_WEB_ROOT);
  const fs::path index_html = web_root / "index.html";

  drogon::app().setLogLevel(log_level);
  drogon::app().setDocumentRoot(web_root.string());

  register_health_routes();
  register_model_routes(runtime_state);
  register_chat_routes(runtime_state);
  register_mcp_routes(runtime_state);
  register_deferred_routes();
  register_spa_routes(web_root, index_html);

  LOG_INFO << "Starting server on " << host << ":" << port;

  drogon::app().registerPreRoutingAdvice([&config](const drogon::HttpRequestPtr &req, drogon::FilterCallback &&defer, drogon::FilterChainCallback &&chain) {
    auto origin = req->getHeader("origin");
    if (!origin.empty()) {
      bool allowed = false;
      for (const auto& allowed_origin : config.allowed_origins) {
        if (origin == allowed_origin) {
          allowed = true;
          break;
        }
      }
      if (!allowed) {
        LOG_WARN << "CORS origin rejected: " << origin;
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k403Forbidden);
        resp->setBody("Forbidden Origin");
        defer(resp);
        return;
      }
    }
    
    if (req->method() == drogon::Options) {
      auto resp = drogon::HttpResponse::newHttpResponse();
      if (!origin.empty()) {
        resp->addHeader("Access-Control-Allow-Origin", origin);
        resp->addHeader("Access-Control-Allow-Methods", "OPTIONS, GET, POST, DELETE, PUT");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, X-Correlation-Id");
      }
      defer(resp);
      return;
    }
    chain();
  });

  drogon::app().registerPostHandlingAdvice([&config](const drogon::HttpRequestPtr &req, const drogon::HttpResponsePtr &resp) {
    auto origin = req->getHeader("origin");
    if (!origin.empty()) {
       resp->addHeader("Access-Control-Allow-Origin", origin);
    }
  });

  drogon::app().addListener(host, port);
  drogon::app().run();

  LOG_INFO << "Server stopping, waiting for background tasks...";
  shutdown_chat_routes();
  LOG_INFO << "Server stopped.";

  return 0;
}
