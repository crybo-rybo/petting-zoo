#include "routes.hpp"

#include <drogon/drogon.h>

#include "http_helpers.hpp"
#include "runtime_state.hpp"

#ifdef ZOO_ENABLE_MCP

namespace {



Json::Value serialize_mcp_entry(const McpConnectorEntry &entry) {
  Json::Value val;
  val["id"] = entry.id;
  val["command"] = entry.config.transport.command;
  Json::Value args(Json::arrayValue);
  for (const auto &arg : entry.config.transport.args) {
    args.append(arg);
  }
  val["args"] = args;
  return val;
}

Json::Value serialize_mcp_summary(const zoo::Agent::McpServerSummary &summary) {
  Json::Value val;
  val["server_id"] = summary.server_id;
  val["connected"] = summary.connected;
  val["discovered_tool_count"] = static_cast<Json::UInt64>(summary.discovered_tool_count);
  return val;
}

void list_mcp_connectors(RuntimeState &state, const drogon::HttpRequestPtr &req,
                         std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
  LOG_INFO << "Listing MCP connectors";
  auto connectors = state.list_mcp_connectors();
  
  Json::Value connectors_arr(Json::arrayValue);
  for (const auto &conn : connectors) {
    connectors_arr.append(serialize_mcp_entry(conn));
  }
  
  Json::Value root;
  root["connectors"] = connectors_arr;
  auto resp = drogon::HttpResponse::newHttpResponse();
  write_json(req, resp, root);
  cb(resp);
}



void connect_mcp_server(RuntimeState &state, const drogon::HttpRequestPtr &req,
                        std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                        const std::string &connector_id) {
  LOG_INFO << "Connecting MCP server: " << connector_id;
  std::string error_code, error_message;
  auto summary = state.connect_mcp_server(connector_id, error_code, error_message);
  if (!summary) {
    LOG_ERROR << "Failed to connect MCP server " << connector_id << ": " << error_message;
    auto status = error_code == "APP-MCP-404" ? drogon::k404NotFound : drogon::k500InternalServerError;
    write_error(req, std::move(cb), status, error_code, "internal", error_message, true);
    return;
  }

  auto resp = drogon::HttpResponse::newHttpResponse();
  write_json(req, resp, serialize_mcp_summary(*summary));
  cb(resp);
}

void disconnect_mcp_server(RuntimeState &state, const drogon::HttpRequestPtr &req,
                           std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                           const std::string &connector_id) {
  LOG_INFO << "Disconnecting MCP server: " << connector_id;
  std::string error_code, error_message;
  bool disconnected = state.disconnect_mcp_server(connector_id, error_code, error_message);
  if (!disconnected) {
    LOG_ERROR << "Failed to disconnect MCP server " << connector_id << ": " << error_message;
    auto status = error_code == "APP-MCP-404" ? drogon::k404NotFound : drogon::k500InternalServerError;
    write_error(req, std::move(cb), status, error_code, "internal", error_message, true);
    return;
  }

  Json::Value root;
  root["status"] = "disconnected";
  root["server_id"] = connector_id;
  auto resp = drogon::HttpResponse::newHttpResponse();
  write_json(req, resp, root);
  cb(resp);
}

} // namespace

void register_mcp_routes(RuntimeState &state) {
  drogon::app().registerHandler(
      "/api/mcp/connectors",
      [&state](const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        if (req->method() == drogon::Get) {
          list_mcp_connectors(state, req, std::move(cb));
        }
      },
      {drogon::Get});

  drogon::app().registerHandler(
      "/api/mcp/connectors/{1}/connect",
      [&state](const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb,
               const std::string &id) {
        connect_mcp_server(state, req, std::move(cb), id);
      },
      {drogon::Post});

  drogon::app().registerHandler(
      "/api/mcp/connectors/{1}/disconnect",
      [&state](const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb,
               const std::string &id) {
        disconnect_mcp_server(state, req, std::move(cb), id);
      },
      {drogon::Post});
}

#else

void register_mcp_routes(RuntimeState & /*state*/) {
  // MCP not enabled in build, register nothing, fallback to deferred or 404
}

#endif
