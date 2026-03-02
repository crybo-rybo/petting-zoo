#include <drogon/drogon.h>
#include <json/json.h>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <string>
#include <string_view>
#include <chrono>
#include <mutex>
#include <unordered_map>

#include "http_helpers.hpp"
#include "routes.hpp"
#include "runtime_state.hpp"

namespace {

struct RateLimitBucket {
  std::chrono::steady_clock::time_point window_start{};
  int request_count = 0;
};

class SlidingWindowRateLimiter {
 public:
  bool allow(const std::string &key, int window_seconds, int max_requests) {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mu_);
    auto &bucket = buckets_[key];
    if (bucket.window_start.time_since_epoch().count() == 0 ||
        now - bucket.window_start >= std::chrono::seconds(window_seconds)) {
      bucket.window_start = now;
      bucket.request_count = 0;
    }
    bucket.request_count += 1;
    return bucket.request_count <= max_requests;
  }

 private:
  std::mutex mu_;
  std::unordered_map<std::string, RateLimitBucket> buckets_;
};

drogon::HttpResponsePtr build_json_error(const drogon::HttpRequestPtr &req,
                                         drogon::HttpStatusCode status,
                                         std::string code,
                                         std::string category,
                                         std::string message,
                                         bool retryable) {
  Json::Value payload(Json::objectValue);
  payload["code"] = std::move(code);
  payload["category"] = std::move(category);
  payload["message"] = std::move(message);
  payload["retryable"] = retryable;
  payload["correlation_id"] = resolve_correlation_id(req);

  Json::Value root(Json::objectValue);
  root["error"] = payload;

  auto resp = drogon::HttpResponse::newHttpResponse();
  write_json(req, resp, root, status);
  return resp;
}

bool route_starts_with(const std::string &route, std::string_view prefix) {
  return route.size() >= prefix.size() &&
         std::equal(prefix.begin(), prefix.end(), route.begin());
}

}  // namespace

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
    if (server.isMember("max_request_body_bytes") && server["max_request_body_bytes"].isUInt64()) {
      config.max_request_body_bytes = static_cast<std::size_t>(server["max_request_body_bytes"].asUInt64());
    }
    if (server.isMember("max_chat_message_chars") && server["max_chat_message_chars"].isUInt64()) {
      config.max_chat_message_chars = static_cast<std::size_t>(server["max_chat_message_chars"].asUInt64());
    }
    if (server.isMember("rate_limit") && server["rate_limit"].isObject()) {
      const auto &rate_limit = server["rate_limit"];
      if (rate_limit.isMember("enabled") && rate_limit["enabled"].isBool()) {
        config.rate_limit_enabled = rate_limit["enabled"].asBool();
      }
      if (rate_limit.isMember("window_seconds") && rate_limit["window_seconds"].isInt()) {
        config.rate_limit_window_seconds = rate_limit["window_seconds"].asInt();
      }
      if (rate_limit.isMember("max_requests") && rate_limit["max_requests"].isInt()) {
        config.rate_limit_max_requests = rate_limit["max_requests"].asInt();
      }
      if (rate_limit.isMember("chat_max_requests") && rate_limit["chat_max_requests"].isInt()) {
        config.rate_limit_chat_max_requests = rate_limit["chat_max_requests"].asInt();
      }
    }
  }
  if (config.max_request_body_bytes == 0) {
    config.max_request_body_bytes = 1024 * 1024;
  }
  if (config.max_chat_message_chars == 0) {
    config.max_chat_message_chars = 16000;
  }
  if (config.rate_limit_window_seconds <= 0) {
    config.rate_limit_window_seconds = 60;
  }
  if (config.rate_limit_max_requests <= 0) {
    config.rate_limit_max_requests = 120;
  }
  if (config.rate_limit_chat_max_requests <= 0) {
    config.rate_limit_chat_max_requests = 30;
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
    if (runtime.isMember("memory_db_path") && runtime["memory_db_path"].isString()) {
      config.memory_db_path = runtime["memory_db_path"].asString();
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

int main(int argc, char** argv) {
  namespace fs = std::filesystem;

  int port = 8080;
  std::string host = "127.0.0.1";
  auto log_level = trantor::Logger::kWarn;

  RuntimeConfig config = load_config("config/app.json", port, host, log_level);
  const fs::path executable_dir =
      (argc > 0 && argv != nullptr && argv[0] != nullptr && std::strlen(argv[0]) > 0)
          ? fs::absolute(fs::path(argv[0])).parent_path()
          : fs::current_path();
  if (!fs::path(config.memory_db_path).is_absolute()) {
    config.memory_db_path = (executable_dir / config.memory_db_path).lexically_normal().string();
  }
  const fs::path memory_db_parent = fs::path(config.memory_db_path).parent_path();
  if (!memory_db_parent.empty()) {
    std::error_code ec;
    fs::create_directories(memory_db_parent, ec);
  }

  static RuntimeState runtime_state(config);

  const fs::path web_root = fs::path(PETTING_ZOO_WEB_ROOT);
  const fs::path index_html = web_root / "index.html";

  drogon::app().setLogLevel(log_level);
  drogon::app().setDocumentRoot(web_root.string());
  drogon::app().setClientMaxBodySize(config.max_request_body_bytes);

  register_health_routes();
  register_model_routes(runtime_state);
  register_chat_routes(runtime_state);
  register_mcp_routes(runtime_state);
  register_deferred_routes();
  register_spa_routes(web_root, index_html);

  LOG_INFO << "Starting server on " << host << ":" << port;

  static SlidingWindowRateLimiter rate_limiter;
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
        defer(build_json_error(req, drogon::k403Forbidden, "APP-SEC-403", "auth",
                               "Forbidden Origin", false));
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

    if (req->body().size() > config.max_request_body_bytes) {
      LOG_WARN << "Request body too large, size=" << req->body().size();
      defer(build_json_error(req, drogon::k413RequestEntityTooLarge, "APP-REQ-413",
                             "validation", "Request payload exceeds configured limit", false));
      return;
    }

    if (config.rate_limit_enabled) {
      const auto route = req->path();
      const bool chat_route =
          route_starts_with(route, "/api/chat/complete") || route_starts_with(route, "/api/chat/stream");
      const int max_requests =
          chat_route ? config.rate_limit_chat_max_requests : config.rate_limit_max_requests;
      const auto client_ip = req->peerAddr().toIp();
      const std::string bucket_key = client_ip + "|" + (chat_route ? "chat" : "default");
      if (!rate_limiter.allow(bucket_key, config.rate_limit_window_seconds, max_requests)) {
        LOG_WARN << "Rate limit exceeded for " << client_ip << " route=" << route;
        defer(build_json_error(req, drogon::k429TooManyRequests, "APP-RATE-429",
                               "rate_limit", "Too many requests", true));
        return;
      }
    }

    chain();
  });

  drogon::app().registerPostHandlingAdvice([&config](const drogon::HttpRequestPtr &req, const drogon::HttpResponsePtr &resp) {
    auto origin = req->getHeader("origin");
    if (!origin.empty()) {
      for (const auto& allowed_origin : config.allowed_origins) {
        if (origin == allowed_origin) {
          resp->addHeader("Access-Control-Allow-Origin", origin);
          break;
        }
      }
    }
  });

  drogon::app().addListener(host, port);
  drogon::app().run();

  LOG_INFO << "Server stopping, waiting for background tasks...";
  shutdown_chat_routes();
  LOG_INFO << "Server stopped.";

  return 0;
}
