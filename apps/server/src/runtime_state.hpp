#pragma once

#include <optional>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <zoo/agent.hpp>
#include <zoo/engine/context_database.hpp>
#ifdef ZOO_ENABLE_MCP
#include <zoo/mcp/mcp_client.hpp>
#endif

struct ModelEntry {
  std::string id;
  std::string display_name;
  std::string path;
  std::string status = "available";
  int context_size = 2048;
  std::uintmax_t file_size_bytes = 0;
};

struct ParsedModelRegisterRequest {
  std::string path;
  std::optional<std::string> display_name;
};

#ifdef ZOO_ENABLE_MCP
struct McpConnectorEntry {
  std::string id;
  zoo::mcp::McpClient::Config config;
};


#endif

struct RuntimeConfig {
  std::vector<std::string> model_discovery_paths = {"./uploads"};
  std::vector<std::string> allowed_origins = {"http://127.0.0.1:8080", "http://localhost:8080"};
#ifdef ZOO_ENABLE_MCP
  std::vector<McpConnectorEntry> mcp_connectors;
#endif
};

class RuntimeState {
 public:
  explicit RuntimeState(RuntimeConfig config = {});

  std::vector<ModelEntry> list_models() const;
  std::optional<std::string> active_model_id() const;

  std::optional<ModelEntry> register_model(const ParsedModelRegisterRequest &req,
                                           std::string &error_code,
                                           std::string &error_message);

  std::optional<ModelEntry> select_model(const std::string &model_id,
                                         std::optional<int> context_size_override,
                                         std::string &error_code,
                                         std::string &error_message);

  void unload_model();

  std::optional<zoo::Response> chat_complete(const std::string &message,
                                             std::string &error_code,
                                             std::string &error_message);

  std::optional<std::string> reset_chat(std::string &error_code,
                                        std::string &error_message);

  std::optional<std::string> clear_memory(std::string &error_code,
                                          std::string &error_message);

  std::optional<zoo::Response> chat_stream(const std::string &message,
                                           std::function<void(std::string_view)> token_callback,
                                           std::string &error_code,
                                           std::string &error_message);

#ifdef ZOO_ENABLE_MCP
  std::vector<McpConnectorEntry> list_mcp_connectors() const;



  std::optional<zoo::Agent::McpServerSummary> connect_mcp_server(const std::string &id,
                                                                 std::string &error_code,
                                                                 std::string &error_message);

  bool disconnect_mcp_server(const std::string &id,
                             std::string &error_code,
                             std::string &error_message);
#endif

 private:
  mutable std::mutex mu_;
  mutable std::mutex agent_mu_;  // Serializes agent operations (chat, reset)
  std::unordered_map<std::string, ModelEntry> models_;
  std::optional<std::string> active_model_id_;
  std::shared_ptr<zoo::Agent> agent_;
  std::shared_ptr<zoo::engine::ContextDatabase> context_db_;
#ifdef ZOO_ENABLE_MCP
  std::unordered_map<std::string, McpConnectorEntry> mcp_connectors_;
#endif
  RuntimeConfig config_;
};

std::string sanitize_model_id(std::string input);
