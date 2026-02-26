#pragma once

#include <optional>
#include <string>
#include <memory>

#include <json/json.h>

#include "runtime_state.hpp"

using JsonPtr = std::shared_ptr<Json::Value>;

std::optional<std::string> parse_model_register_request(const JsonPtr &json,
                                                        ParsedModelRegisterRequest &out,
                                                        Json::Value &details);

std::optional<std::string> parse_model_select_request(const JsonPtr &json,
                                                      std::string &model_id,
                                                      std::optional<int> &context_size,
                                                      Json::Value &details);

std::optional<std::string> parse_chat_complete_request(const JsonPtr &json,
                                                       std::string &message,
                                                       Json::Value &details);
