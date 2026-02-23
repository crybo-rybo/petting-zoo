#include "runtime_state.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <mutex>
#include <unordered_map>

std::string sanitize_model_id(std::string input) {
  for (char &ch : input) {
    const auto uch = static_cast<unsigned char>(ch);
    if (std::isalnum(uch)) {
      ch = static_cast<char>(std::tolower(uch));
    } else {
      ch = '-';
    }
  }

  const auto first = input.find_first_not_of('-');
  if (first == std::string::npos) {
    return "model";
  }
  const auto last = input.find_last_not_of('-');
  return input.substr(first, last - first + 1);
}

RuntimeState::RuntimeState() {
  auto db_result = zoo::engine::ContextDatabase::open("uploads/memory.db");
  if (db_result) {
    context_db_ = std::move(*db_result);
  }
}

std::vector<ModelEntry> RuntimeState::list_models() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<ModelEntry> out;
  out.reserve(models_.size());
  for (const auto &item : models_) {
    auto model = item.second;
    model.status = std::filesystem::exists(model.path) ? "available" : "unavailable";
    out.push_back(std::move(model));
  }
  std::sort(out.begin(), out.end(), [](const ModelEntry &a, const ModelEntry &b) {
    return a.display_name < b.display_name;
  });
  return out;
}

std::optional<std::string> RuntimeState::active_model_id() const {
  std::lock_guard<std::mutex> lock(mu_);
  return active_model_id_;
}

std::optional<ModelEntry> RuntimeState::register_model(
    const ParsedModelRegisterRequest &req, std::string &error_code,
    std::string &error_message) {
  namespace fs = std::filesystem;
  const fs::path model_path = fs::path(req.path).lexically_normal();
  if (!fs::exists(model_path) || !fs::is_regular_file(model_path)) {
    error_code = "APP-VAL-001";
    error_message = "Model path does not exist or is not a regular file";
    return std::nullopt;
  }

  const std::string display_name = req.display_name.value_or(model_path.filename().string());
  std::string id = sanitize_model_id(model_path.stem().string());
  if (id.empty()) {
    id = "model";
  }

  std::lock_guard<std::mutex> lock(mu_);
  if (models_.contains(id) && models_[id].path != model_path.string()) {
    int suffix = 2;
    std::string base = id;
    do {
      id = base + "-" + std::to_string(suffix++);
    } while (models_.contains(id));
  }

  ModelEntry model;
  model.id = id;
  model.display_name = display_name;
  model.path = model_path.string();
  model.status = "available";
  models_[model.id] = model;
  return model;
}

std::optional<ModelEntry> RuntimeState::select_model(const std::string &model_id,
                                                     std::string &error_code,
                                                     std::string &error_message) {
  ModelEntry selected;
  {
    std::lock_guard<std::mutex> lock(mu_);
    const auto it = models_.find(model_id);
    if (it == models_.end()) {
      error_code = "APP-MOD-404";
      error_message = "Model not found";
      return std::nullopt;
    }
    selected = it->second;
  }

  if (!std::filesystem::exists(selected.path)) {
    error_code = "APP-VAL-001";
    error_message = "Model path is no longer available";
    return std::nullopt;
  }

  zoo::Config config;
  config.model_path = selected.path;
  config.context_size = selected.context_size;
  config.max_tokens = 512;

  auto created = zoo::Agent::create(config);
  if (!created) {
    error_code = "APP-UPSTREAM-001";
    error_message = created.error().to_string();
    return std::nullopt;
  }

  std::shared_ptr<zoo::Agent> loaded = std::shared_ptr<zoo::Agent>(std::move(*created));
  
  if (context_db_) {
    loaded->set_context_database(context_db_);
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    agent_ = std::move(loaded);
    active_model_id_ = selected.id;
  }
  return selected;
}

std::optional<zoo::Response> RuntimeState::chat_complete(const std::string &message,
                                                         std::string &error_code,
                                                         std::string &error_message) {
  std::shared_ptr<zoo::Agent> agent;
  {
    std::lock_guard<std::mutex> lock(mu_);
    agent = agent_;
  }
  if (!agent) {
    error_code = "APP-STATE-409";
    error_message = "No active model is loaded";
    return std::nullopt;
  }

  std::lock_guard<std::mutex> agent_lock(agent_mu_);
  auto handle = agent->chat(zoo::Message::user(message));
  auto result = handle.future.get();
  if (!result) {
    error_code = "APP-UPSTREAM-001";
    error_message = result.error().to_string();
    return std::nullopt;
  }
  return *result;
}

std::optional<zoo::Response> RuntimeState::chat_stream(
    const std::string &message,
    std::function<void(std::string_view)> token_callback,
    std::string &error_code,
    std::string &error_message) {
  std::shared_ptr<zoo::Agent> agent;
  {
    std::lock_guard<std::mutex> lock(mu_);
    agent = agent_;
  }
  if (!agent) {
    error_code = "APP-STATE-409";
    error_message = "No active model is loaded";
    return std::nullopt;
  }

  std::lock_guard<std::mutex> agent_lock(agent_mu_);
  auto handle = agent->chat(zoo::Message::user(message), std::move(token_callback));
  auto result = handle.future.get();
  if (!result) {
    error_code = "APP-UPSTREAM-001";
    error_message = result.error().to_string();
    return std::nullopt;
  }
  return *result;
}

std::optional<std::string> RuntimeState::reset_chat(std::string &error_code,
                                                     std::string &error_message) {
  std::shared_ptr<zoo::Agent> agent;
  std::optional<std::string> model_id;
  {
    std::lock_guard<std::mutex> lock(mu_);
    agent = agent_;
    model_id = active_model_id_;
  }
  if (!agent || !model_id.has_value()) {
    error_code = "APP-STATE-409";
    error_message = "No active model is loaded";
    return std::nullopt;
  }

  std::lock_guard<std::mutex> agent_lock(agent_mu_);
  agent->clear_history();
  return model_id;
}

void RuntimeState::unload_model() {
  std::lock_guard<std::mutex> lock(mu_);
  std::lock_guard<std::mutex> agent_lock(agent_mu_);
  agent_.reset();
  active_model_id_ = std::nullopt;
}

std::optional<std::string> RuntimeState::clear_memory(std::string &error_code,
                                                      std::string &error_message) {
  std::lock_guard<std::mutex> lock(mu_);
  
  if (!context_db_) {
    error_code = "APP-STATE-500";
    error_message = "Memory database is not initialized";
    return std::nullopt;
  }
  
  // Close the old database
  context_db_.reset();

  // Wipe the actual file
  std::filesystem::remove("uploads/memory.db");

  // Re-initialize the database
  auto db_result = zoo::engine::ContextDatabase::open("uploads/memory.db");
  if (db_result) {
    context_db_ = std::move(*db_result);
  } else {
    error_code = "APP-DB-500";
    error_message = "Failed to recreate memory database";
    return std::nullopt;
  }

  // If there's an active agent, we need to update its reference
  if (agent_) {
    std::lock_guard<std::mutex> agent_lock(agent_mu_);
    agent_->set_context_database(context_db_);
  }

  return active_model_id_.value_or("none");
}

#ifdef ZOO_ENABLE_MCP
std::vector<McpConnectorEntry> RuntimeState::list_mcp_connectors() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<McpConnectorEntry> out;
  out.reserve(mcp_connectors_.size());
  for (const auto &kv : mcp_connectors_) {
    out.push_back(kv.second);
  }
  return out;
}

std::optional<McpConnectorEntry> RuntimeState::add_mcp_connector(
    const ParsedMcpConnectRequest &req, std::string &error_code,
    std::string &error_message) {
  std::lock_guard<std::mutex> lock(mu_);
  if (mcp_connectors_.contains(req.id)) {
    error_code = "APP-MCP-409";
    error_message = "Connector ID already exists";
    return std::nullopt;
  }

  McpConnectorEntry entry;
  entry.id = req.id;
  entry.config.server_id = req.id;
  entry.config.transport.command = req.command;
  entry.config.transport.args = req.args;

  mcp_connectors_[req.id] = entry;
  return entry;
}

bool RuntimeState::remove_mcp_connector(const std::string &id,
                                        std::string &error_code,
                                        std::string &error_message) {
  std::lock_guard<std::mutex> lock(mu_);
  if (!mcp_connectors_.contains(id)) {
    error_code = "APP-MCP-404";
    error_message = "Connector not found";
    return false;
  }
  
  // If we have an active agent, try to disconnect it just in case
  if (agent_) {
    std::lock_guard<std::mutex> agent_lock(agent_mu_);
    (void)agent_->remove_mcp_server(id);
  }

  mcp_connectors_.erase(id);
  return true;
}

std::optional<zoo::Agent::McpServerSummary> RuntimeState::connect_mcp_server(
    const std::string &id, std::string &error_code, std::string &error_message) {
  McpConnectorEntry entry;
  std::shared_ptr<zoo::Agent> agent;
  {
    std::lock_guard<std::mutex> lock(mu_);
    const auto it = mcp_connectors_.find(id);
    if (it == mcp_connectors_.end()) {
      error_code = "APP-MCP-404";
      error_message = "Connector not found";
      return std::nullopt;
    }
    entry = it->second;
    agent = agent_;
  }

  if (!agent) {
    error_code = "APP-STATE-409";
    error_message = "No active model is loaded (cannot connect MCP tools without agent)";
    return std::nullopt;
  }

  std::lock_guard<std::mutex> agent_lock(agent_mu_);
  auto result = agent->add_mcp_server(entry.config);
  if (!result) {
    error_code = "APP-UPSTREAM-001";
    error_message = result.error().to_string();
    return std::nullopt;
  }

  auto summary = agent->get_mcp_server(id);
  if (!summary) {
    error_code = "APP-UPSTREAM-002";
    error_message = "Failed to fetch summary after connection";
    return std::nullopt; 
  }
  return *summary;
}

bool RuntimeState::disconnect_mcp_server(const std::string &id,
                                         std::string &error_code,
                                         std::string &error_message) {
  std::shared_ptr<zoo::Agent> agent;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!mcp_connectors_.contains(id)) {
      error_code = "APP-MCP-404";
      error_message = "Connector not found";
      return false;
    }
    agent = agent_;
  }

  if (!agent) {
    // If agent is gone, then it's effectively disconnected
    return true; 
  }

  std::lock_guard<std::mutex> agent_lock(agent_mu_);
  auto result = agent->remove_mcp_server(id);
  if (!result) {
    error_code = "APP-UPSTREAM-001";
    error_message = result.error().to_string();
    return false;
  }
  return true;
}
#endif
