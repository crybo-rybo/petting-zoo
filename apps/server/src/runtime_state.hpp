#pragma once

#include <optional>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <zoo/agent.hpp>

struct ModelEntry {
  std::string id;
  std::string display_name;
  std::string path;
  std::string status = "available";
  int context_size = 8192;
};

struct ParsedModelRegisterRequest {
  std::string path;
  std::optional<std::string> display_name;
};

class RuntimeState {
 public:
  std::vector<ModelEntry> list_models() const;
  std::optional<std::string> active_model_id() const;

  std::optional<ModelEntry> register_model(const ParsedModelRegisterRequest &req,
                                           std::string &error_code,
                                           std::string &error_message);

  std::optional<ModelEntry> select_model(const std::string &model_id,
                                         std::string &error_code,
                                         std::string &error_message);

  std::optional<zoo::Response> chat_complete(const std::string &message,
                                             std::string &error_code,
                                             std::string &error_message);

  std::optional<std::string> reset_chat(std::string &error_code,
                                        std::string &error_message);

  std::optional<zoo::Response> chat_stream(const std::string &message,
                                           std::function<void(std::string_view)> token_callback,
                                           std::string &error_code,
                                           std::string &error_message);

 private:
  mutable std::mutex mu_;
  mutable std::mutex agent_mu_;  // Serializes agent operations (chat, reset)
  std::unordered_map<std::string, ModelEntry> models_;
  std::optional<std::string> active_model_id_;
  std::shared_ptr<zoo::Agent> agent_;
};

std::string sanitize_model_id(std::string input);
