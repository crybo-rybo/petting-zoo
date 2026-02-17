#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <zoo/mcp/mcp_client.hpp>

namespace {

using JsonPtr = std::shared_ptr<Json::Value>;

enum class TransportKind {
  Stdio,
  HttpStream,
};

struct ToolSummary {
  std::string name;
  std::string description;
  Json::Value input_schema{Json::objectValue};
};

struct Connector {
  std::string id;
  std::string name;
  TransportKind transport = TransportKind::Stdio;
  std::string status = "disconnected";
  std::string protocol_version = "2025-06-18";
  std::string command;
  std::vector<std::string> args;
  std::string endpoint;
  Json::Value capabilities{Json::objectValue};
  std::string created_at;
  std::optional<std::string> updated_at;
  std::vector<ToolSummary> tools;
  std::shared_ptr<zoo::mcp::McpClient> client;
};

struct TemplateEntry {
  std::string id;
  std::string name;
  std::string description;
  TransportKind transport = TransportKind::Stdio;
  std::string command;
  std::vector<std::string> args;
  std::string endpoint;
  std::vector<std::string> required_fields;
};

std::string now_rfc3339_utc() {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const auto secs = floor<seconds>(now);
  const auto ms = duration_cast<milliseconds>(now - secs).count();

  std::time_t tt = system_clock::to_time_t(now);
  std::tm utc_tm{};
#if defined(_WIN32)
  gmtime_s(&utc_tm, &tt);
#else
  gmtime_r(&tt, &utc_tm);
#endif

  std::ostringstream os;
  os << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%S") << "." << std::setw(3)
     << std::setfill('0') << ms << "Z";
  return os.str();
}

std::string generate_correlation_id() {
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  static constexpr char chars[] = "0123456789abcdefghijklmnopqrstuvwxyz";

  std::uniform_int_distribution<int> pick(0, 35);
  std::string out = "cor_";
  out.reserve(24);
  for (int i = 0; i < 20; ++i) {
    out.push_back(chars[pick(rng)]);
  }
  return out;
}

std::string generate_connector_id() {
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  static constexpr char chars[] = "0123456789abcdefghijklmnopqrstuvwxyz";

  std::uniform_int_distribution<int> pick(0, 35);
  std::string out = "mcp_";
  out.reserve(16);
  for (int i = 0; i < 12; ++i) {
    out.push_back(chars[pick(rng)]);
  }
  return out;
}

std::string resolve_correlation_id(const drogon::HttpRequestPtr &req) {
  const auto incoming = req->getHeader("X-Correlation-Id");
  if (!incoming.empty()) {
    return incoming;
  }
  return generate_correlation_id();
}

void write_json(const drogon::HttpRequestPtr &req,
                const drogon::HttpResponsePtr &resp,
                const Json::Value &json,
                drogon::HttpStatusCode code = drogon::k200OK) {
  const auto cid = resolve_correlation_id(req);
  resp->setStatusCode(code);
  resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  resp->addHeader("X-Correlation-Id", cid);
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  resp->setBody(Json::writeString(builder, json));
}

void write_error(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                 drogon::HttpStatusCode status,
                 std::string code,
                 std::string category,
                 std::string message,
                 bool retryable,
                 const std::optional<Json::Value> &details = std::nullopt) {
  Json::Value payload(Json::objectValue);
  payload["code"] = std::move(code);
  payload["category"] = std::move(category);
  payload["message"] = std::move(message);
  payload["retryable"] = retryable;
  payload["correlation_id"] = resolve_correlation_id(req);
  if (details.has_value()) {
    payload["details"] = *details;
  }

  Json::Value error(Json::objectValue);
  error["error"] = payload;

  auto resp = drogon::HttpResponse::newHttpResponse();
  write_json(req, resp, error, status);
  cb(resp);
}

bool starts_with(const std::string &value, const std::string &prefix) {
  return value.rfind(prefix, 0) == 0;
}

std::string transport_to_string(TransportKind transport) {
  return transport == TransportKind::Stdio ? "stdio" : "http_stream";
}

std::optional<TransportKind> parse_transport(const std::string &raw) {
  if (raw == "stdio") {
    return TransportKind::Stdio;
  }
  if (raw == "http_stream") {
    return TransportKind::HttpStream;
  }
  return std::nullopt;
}

Json::Value connector_to_json(const Connector &connector) {
  Json::Value out(Json::objectValue);
  out["id"] = connector.id;
  out["name"] = connector.name;
  out["transport"] = transport_to_string(connector.transport);
  out["status"] = connector.status;
  out["protocol_version"] = connector.protocol_version;
  out["command"] = connector.command;

  Json::Value args(Json::arrayValue);
  for (const auto &arg : connector.args) {
    args.append(arg);
  }
  out["args"] = args;
  out["endpoint"] = connector.endpoint;
  out["capabilities"] = connector.capabilities;
  out["created_at"] = connector.created_at;
  if (connector.updated_at.has_value()) {
    out["updated_at"] = *connector.updated_at;
  } else {
    out["updated_at"] = Json::nullValue;
  }
  return out;
}

Json::Value tool_to_json(const ToolSummary &tool) {
  Json::Value out(Json::objectValue);
  out["name"] = tool.name;
  out["description"] = tool.description;
  out["input_schema"] = tool.input_schema;
  return out;
}

Json::Value template_to_json(const TemplateEntry &entry) {
  Json::Value out(Json::objectValue);
  out["id"] = entry.id;
  out["name"] = entry.name;
  out["description"] = entry.description;
  out["transport"] = transport_to_string(entry.transport);

  Json::Value defaults(Json::objectValue);
  defaults["command"] = entry.command;
  Json::Value args(Json::arrayValue);
  for (const auto &arg : entry.args) {
    args.append(arg);
  }
  defaults["args"] = args;
  defaults["endpoint"] = entry.endpoint;
  out["defaults"] = defaults;

  Json::Value required(Json::arrayValue);
  for (const auto &field : entry.required_fields) {
    required.append(field);
  }
  out["required_fields"] = required;
  return out;
}

Json::Value jsoncpp_from_nlohmann(const nlohmann::json &value) {
  Json::CharReaderBuilder builder;
  Json::Value out;
  std::string errs;
  std::istringstream input(value.dump());
  if (Json::parseFromStream(builder, input, &out, &errs)) {
    return out;
  }
  return Json::nullValue;
}

struct ParsedConnectorRequest {
  std::string name;
  TransportKind transport = TransportKind::Stdio;
  std::string protocol_version = "2025-06-18";
  std::string command;
  std::vector<std::string> args;
  std::string endpoint;
};

std::optional<std::string> parse_connector_request(const JsonPtr &json,
                                                   ParsedConnectorRequest &out,
                                                   Json::Value &details) {
  if (!json) {
    return "Missing JSON body";
  }
  if (!json->isObject()) {
    return "Body must be a JSON object";
  }

  const auto &obj = *json;
  if (!obj.isMember("name") || !obj["name"].isString()) {
    details["field"] = "name";
    return "Field 'name' is required and must be a string";
  }
  out.name = obj["name"].asString();
  if (out.name.empty()) {
    details["field"] = "name";
    return "Field 'name' cannot be empty";
  }

  if (!obj.isMember("transport") || !obj["transport"].isString()) {
    details["field"] = "transport";
    return "Field 'transport' is required and must be a string";
  }
  const auto transport = parse_transport(obj["transport"].asString());
  if (!transport.has_value()) {
    details["field"] = "transport";
    return "Field 'transport' must be 'stdio' or 'http_stream'";
  }
  out.transport = *transport;

  if (obj.isMember("protocol_version")) {
    if (!obj["protocol_version"].isString()) {
      details["field"] = "protocol_version";
      return "Field 'protocol_version' must be a string";
    }
    out.protocol_version = obj["protocol_version"].asString();
  }

  if (obj.isMember("command")) {
    if (!obj["command"].isString()) {
      details["field"] = "command";
      return "Field 'command' must be a string";
    }
    out.command = obj["command"].asString();
  }

  if (obj.isMember("args")) {
    if (!obj["args"].isArray()) {
      details["field"] = "args";
      return "Field 'args' must be an array of strings";
    }
    for (const auto &arg : obj["args"]) {
      if (!arg.isString()) {
        details["field"] = "args";
        return "All entries in 'args' must be strings";
      }
      out.args.push_back(arg.asString());
    }
  }

  if (obj.isMember("endpoint")) {
    if (!obj["endpoint"].isString()) {
      details["field"] = "endpoint";
      return "Field 'endpoint' must be a string";
    }
    out.endpoint = obj["endpoint"].asString();
  }

  if (out.transport == TransportKind::Stdio && out.command.empty()) {
    details["field"] = "command";
    return "Field 'command' is required for stdio transport";
  }

  if (out.transport == TransportKind::HttpStream && out.endpoint.empty()) {
    details["field"] = "endpoint";
    return "Field 'endpoint' is required for http_stream transport";
  }

  return std::nullopt;
}

Json::Value run_validation_checks(const ParsedConnectorRequest &req) {
  Json::Value checks(Json::arrayValue);

  auto push_check = [&checks](const std::string &name, bool ok,
                              const std::string &message) {
    Json::Value item(Json::objectValue);
    item["name"] = name;
    item["ok"] = ok;
    item["message"] = message;
    checks.append(item);
  };

  push_check("name_not_empty", !req.name.empty(),
             req.name.empty() ? "Connector name must not be empty"
                              : "Connector name is present");

  if (req.transport == TransportKind::Stdio) {
    const bool has_command = !req.command.empty();
    push_check("stdio_command", has_command,
               has_command ? "Command is configured"
                           : "Missing command for stdio connector");
  } else {
    const bool has_endpoint = !req.endpoint.empty();
    push_check("http_endpoint", has_endpoint,
               has_endpoint ? "Endpoint is configured"
                            : "Missing endpoint for http_stream connector");
  }

  return checks;
}

class McpState {
 public:
  McpState()
      : templates_{
            {"filesystem", "Filesystem", "Read/write files through MCP filesystem server",
             TransportKind::Stdio, "npx",
             {"-y", "@modelcontextprotocol/server-filesystem", "."}, "",
             {"name", "transport", "command"}},
            {"fetch", "Fetch", "HTTP fetch and web retrieval connector",
             TransportKind::Stdio, "uvx", {"mcp-server-fetch"}, "",
             {"name", "transport", "command"}},
            {"github", "GitHub", "GitHub API connector via MCP",
             TransportKind::Stdio, "npx",
             {"-y", "@modelcontextprotocol/server-github"}, "",
             {"name", "transport", "command"}},
        } {}

  std::vector<Connector> list_connectors() const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<Connector> out;
    out.reserve(connectors_.size());
    for (const auto &it : connectors_) {
      out.push_back(it.second);
    }
    std::sort(out.begin(), out.end(), [](const Connector &a, const Connector &b) {
      return a.created_at > b.created_at;
    });
    return out;
  }

  std::vector<TemplateEntry> list_templates() const { return templates_; }

  std::optional<Connector> create_connector(const ParsedConnectorRequest &req,
                                            std::string &error_code,
                                            std::string &error_message) {
    std::lock_guard<std::mutex> lock(mu_);

    const auto duplicate = std::find_if(
        connectors_.begin(), connectors_.end(),
        [&req](const auto &item) { return item.second.name == req.name; });
    if (duplicate != connectors_.end()) {
      error_code = "APP-MCP-409";
      error_message = "Connector with the same name already exists";
      return std::nullopt;
    }

    Connector connector;
    connector.id = generate_connector_id();
    connector.name = req.name;
    connector.transport = req.transport;
    connector.protocol_version = req.protocol_version;
    connector.command = req.command;
    connector.args = req.args;
    connector.endpoint = req.endpoint;
    connector.created_at = now_rfc3339_utc();

    connectors_.emplace(connector.id, connector);
    return connector;
  }

  std::optional<Connector> delete_connector(const std::string &id) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = connectors_.find(id);
    if (it == connectors_.end()) {
      return std::nullopt;
    }

    if (it->second.client) {
      it->second.client->disconnect();
      it->second.client.reset();
    }

    auto snapshot = it->second;
    connectors_.erase(it);
    return snapshot;
  }

  std::optional<Connector> connect_connector(const std::string &id,
                                             std::string &error_code,
                                             std::string &error_message) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = connectors_.find(id);
    if (it == connectors_.end()) {
      error_code = "APP-MCP-404";
      error_message = "Connector not found";
      return std::nullopt;
    }

    Connector &connector = it->second;
    if (connector.status == "connected") {
      error_code = "APP-MCP-409";
      error_message = "Connector is already connected";
      return std::nullopt;
    }

    if (connector.transport != TransportKind::Stdio) {
      error_code = "APP-NOT-IMPL-001";
      error_message = "http_stream transport is not implemented yet";
      return std::nullopt;
    }

    zoo::mcp::McpClient::Config config;
    config.server_id = connector.id;
    config.transport.command = connector.command;
    config.transport.args = connector.args;
    config.session.protocol_version = connector.protocol_version;

    auto client_result = zoo::mcp::McpClient::create(config);
    if (!client_result) {
      error_code = "APP-UPSTREAM-002";
      error_message = client_result.error().to_string();
      connector.status = "degraded";
      connector.updated_at = now_rfc3339_utc();
      return std::nullopt;
    }

    auto client = *client_result;
    auto connected = client->connect();
    if (!connected) {
      error_code = "APP-UPSTREAM-002";
      error_message = connected.error().to_string();
      connector.status = "degraded";
      connector.updated_at = now_rfc3339_utc();
      return std::nullopt;
    }

    auto tools_result = client->discover_tools();
    if (!tools_result) {
      error_code = "APP-UPSTREAM-002";
      error_message = tools_result.error().to_string();
      connector.status = "degraded";
      connector.updated_at = now_rfc3339_utc();
      client->disconnect();
      return std::nullopt;
    }

    connector.tools.clear();
    for (const auto &tool : *tools_result) {
      ToolSummary summary;
      summary.name = tool.name;
      summary.description = tool.description;
      summary.input_schema = jsoncpp_from_nlohmann(tool.inputSchema);
      connector.tools.push_back(std::move(summary));
    }

    connector.status = "connected";
    connector.updated_at = now_rfc3339_utc();
    connector.client = client;
    return connector;
  }

  std::optional<Connector> disconnect_connector(const std::string &id,
                                                std::string &error_code,
                                                std::string &error_message) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = connectors_.find(id);
    if (it == connectors_.end()) {
      error_code = "APP-MCP-404";
      error_message = "Connector not found";
      return std::nullopt;
    }

    Connector &connector = it->second;
    if (connector.client) {
      connector.client->disconnect();
      connector.client.reset();
    }
    connector.status = "disconnected";
    connector.updated_at = now_rfc3339_utc();
    return connector;
  }

  std::optional<std::vector<ToolSummary>> refresh_tools(
      const std::string &id, std::string &error_code, std::string &error_message) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = connectors_.find(id);
    if (it == connectors_.end()) {
      error_code = "APP-MCP-404";
      error_message = "Connector not found";
      return std::nullopt;
    }

    Connector &connector = it->second;
    if (!connector.client || !connector.client->is_connected()) {
      error_code = "APP-MCP-409";
      error_message = "Connector is not connected";
      return std::nullopt;
    }

    auto tools_result = connector.client->discover_tools();
    if (!tools_result) {
      error_code = "APP-UPSTREAM-002";
      error_message = tools_result.error().to_string();
      connector.status = "degraded";
      connector.updated_at = now_rfc3339_utc();
      return std::nullopt;
    }

    connector.tools.clear();
    for (const auto &tool : *tools_result) {
      ToolSummary summary;
      summary.name = tool.name;
      summary.description = tool.description;
      summary.input_schema = jsoncpp_from_nlohmann(tool.inputSchema);
      connector.tools.push_back(std::move(summary));
    }

    connector.status = "connected";
    connector.updated_at = now_rfc3339_utc();
    return connector.tools;
  }

  std::optional<std::vector<ToolSummary>> list_tools(const std::string &id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = connectors_.find(id);
    if (it == connectors_.end()) {
      return std::nullopt;
    }
    return it->second.tools;
  }

 private:
  mutable std::mutex mu_;
  std::unordered_map<std::string, Connector> connectors_;
  std::vector<TemplateEntry> templates_;
};

}  // namespace

int main() {
  namespace fs = std::filesystem;

  static McpState mcp_state;

  const fs::path web_root = fs::path(PETTING_ZOO_WEB_ROOT);
  const fs::path index_html = web_root / "index.html";

  drogon::app().setLogLevel(trantor::Logger::kWarn);
  drogon::app().setDocumentRoot(web_root.string());
  drogon::app().setUploadPath("uploads");

  drogon::app().registerHandler(
      "/healthz",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        Json::Value body(Json::objectValue);
        body["status"] = "ok";
        body["service"] = "petting-zoo-server";
        body["version"] = PETTING_ZOO_VERSION;
        body["timestamp"] = now_rfc3339_utc();

        auto resp = drogon::HttpResponse::newHttpResponse();
        write_json(req, resp, body, drogon::k200OK);
        cb(resp);
      });

  drogon::app().registerHandler(
      "/api/mcp/catalog",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        Json::Value body(Json::objectValue);
        Json::Value templates(Json::arrayValue);
        for (const auto &entry : mcp_state.list_templates()) {
          templates.append(template_to_json(entry));
        }
        body["templates"] = templates;

        auto resp = drogon::HttpResponse::newHttpResponse();
        write_json(req, resp, body);
        cb(resp);
      },
      {drogon::Get});

  drogon::app().registerHandler(
      "/api/mcp/connectors",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        Json::Value body(Json::objectValue);
        Json::Value connectors(Json::arrayValue);
        for (const auto &entry : mcp_state.list_connectors()) {
          connectors.append(connector_to_json(entry));
        }
        body["connectors"] = connectors;

        auto resp = drogon::HttpResponse::newHttpResponse();
        write_json(req, resp, body);
        cb(resp);
      },
      {drogon::Get});

  drogon::app().registerHandler(
      "/api/mcp/connectors",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        ParsedConnectorRequest parsed;
        Json::Value details(Json::objectValue);
        if (const auto parse_error =
                parse_connector_request(req->getJsonObject(), parsed, details);
            parse_error.has_value()) {
          write_error(req, std::move(cb), drogon::k400BadRequest, "APP-VAL-001",
                      "validation", *parse_error, false, details);
          return;
        }

        std::string error_code;
        std::string error_message;
        const auto created =
            mcp_state.create_connector(parsed, error_code, error_message);
        if (!created.has_value()) {
          write_error(req, std::move(cb), drogon::k409Conflict, error_code,
                      "conflict", error_message, false);
          return;
        }

        Json::Value body(Json::objectValue);
        body["connector"] = connector_to_json(*created);
        auto resp = drogon::HttpResponse::newHttpResponse();
        write_json(req, resp, body, drogon::k201Created);
        cb(resp);
      },
      {drogon::Post});

  drogon::app().registerHandler(
      "/api/mcp/connectors/validate",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        ParsedConnectorRequest parsed;
        Json::Value details(Json::objectValue);
        if (const auto parse_error =
                parse_connector_request(req->getJsonObject(), parsed, details);
            parse_error.has_value()) {
          write_error(req, std::move(cb), drogon::k400BadRequest, "APP-VAL-001",
                      "validation", *parse_error, false, details);
          return;
        }

        Json::Value body(Json::objectValue);
        body["checks"] = run_validation_checks(parsed);

        bool valid = true;
        for (const auto &check : body["checks"]) {
          if (!check["ok"].asBool()) {
            valid = false;
            break;
          }
        }
        body["valid"] = valid;

        Json::Value warnings(Json::arrayValue);
        if (parsed.transport == TransportKind::HttpStream) {
          warnings.append("http_stream transport is not implemented yet");
        }
        body["warnings"] = warnings;

        auto resp = drogon::HttpResponse::newHttpResponse();
        write_json(req, resp, body);
        cb(resp);
      },
      {drogon::Post});

  drogon::app().registerHandler(
      "/api/mcp/connectors/{1}",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb,
         const std::string &connector_id) {
        const auto deleted = mcp_state.delete_connector(connector_id);
        if (!deleted.has_value()) {
          write_error(req, std::move(cb), drogon::k404NotFound, "APP-MCP-404",
                      "not_found", "Connector not found", false);
          return;
        }

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k204NoContent);
        resp->addHeader("X-Correlation-Id", resolve_correlation_id(req));
        cb(resp);
      },
      {drogon::Delete});

  drogon::app().registerHandler(
      "/api/mcp/connectors/{1}/connect",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb,
         const std::string &connector_id) {
        std::string error_code;
        std::string error_message;
        const auto connector =
            mcp_state.connect_connector(connector_id, error_code, error_message);
        if (!connector.has_value()) {
          const auto status =
              error_code == "APP-MCP-404" ? drogon::k404NotFound : drogon::k409Conflict;
          write_error(req, std::move(cb), status, error_code,
                      status == drogon::k404NotFound ? "not_found" : "conflict",
                      error_message, true);
          return;
        }

        Json::Value body(Json::objectValue);
        body["connector"] = connector_to_json(*connector);
        auto resp = drogon::HttpResponse::newHttpResponse();
        write_json(req, resp, body);
        cb(resp);
      },
      {drogon::Post});

  drogon::app().registerHandler(
      "/api/mcp/connectors/{1}/disconnect",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb,
         const std::string &connector_id) {
        std::string error_code;
        std::string error_message;
        const auto connector =
            mcp_state.disconnect_connector(connector_id, error_code, error_message);
        if (!connector.has_value()) {
          write_error(req, std::move(cb), drogon::k404NotFound, error_code,
                      "not_found", error_message, false);
          return;
        }

        Json::Value body(Json::objectValue);
        body["connector"] = connector_to_json(*connector);
        auto resp = drogon::HttpResponse::newHttpResponse();
        write_json(req, resp, body);
        cb(resp);
      },
      {drogon::Post});

  drogon::app().registerHandler(
      "/api/mcp/connectors/{1}/refresh-tools",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb,
         const std::string &connector_id) {
        std::string error_code;
        std::string error_message;
        const auto tools = mcp_state.refresh_tools(connector_id, error_code, error_message);
        if (!tools.has_value()) {
          const auto status =
              error_code == "APP-MCP-404" ? drogon::k404NotFound : drogon::k409Conflict;
          write_error(req, std::move(cb), status, error_code,
                      status == drogon::k404NotFound ? "not_found" : "conflict",
                      error_message, true);
          return;
        }

        Json::Value body(Json::objectValue);
        Json::Value json_tools(Json::arrayValue);
        for (const auto &tool : *tools) {
          json_tools.append(tool_to_json(tool));
        }
        body["tools"] = json_tools;

        auto resp = drogon::HttpResponse::newHttpResponse();
        write_json(req, resp, body);
        cb(resp);
      },
      {drogon::Post});

  drogon::app().registerHandler(
      "/api/mcp/connectors/{1}/tools",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb,
         const std::string &connector_id) {
        const auto tools = mcp_state.list_tools(connector_id);
        if (!tools.has_value()) {
          write_error(req, std::move(cb), drogon::k404NotFound, "APP-MCP-404",
                      "not_found", "Connector not found", false);
          return;
        }

        Json::Value body(Json::objectValue);
        Json::Value json_tools(Json::arrayValue);
        for (const auto &tool : *tools) {
          json_tools.append(tool_to_json(tool));
        }
        body["tools"] = json_tools;

        auto resp = drogon::HttpResponse::newHttpResponse();
        write_json(req, resp, body);
        cb(resp);
      },
      {drogon::Get});

  drogon::app().registerHandler(
      "/{path:.*}",
      [index_html](const drogon::HttpRequestPtr &req,
                   std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                   const std::string &path) {
        if (starts_with(path, "api/") || path == "api") {
          write_error(req, std::move(cb), drogon::k404NotFound, "APP-NOT-IMPL-001",
                      "internal",
                      "API endpoint not implemented in current application phase",
                      false);
          return;
        }

        if (index_html.empty() || !fs::exists(index_html)) {
          write_error(req, std::move(cb), drogon::k500InternalServerError,
                      "APP-INT-001", "internal",
                      "Web assets missing. Build apps/web to produce dist output.",
                      false);
          return;
        }

        auto resp = drogon::HttpResponse::newFileResponse(index_html.string());
        resp->setContentTypeCode(drogon::CT_TEXT_HTML);
        resp->addHeader("Cache-Control", "no-store");
        resp->addHeader("X-Correlation-Id", resolve_correlation_id(req));
        cb(resp);
      },
      {drogon::Get});

  drogon::app().addListener("127.0.0.1", 8080);
  drogon::app().run();
  return 0;
}
