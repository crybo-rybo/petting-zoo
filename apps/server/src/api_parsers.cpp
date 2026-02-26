#include "api_parsers.hpp"

std::optional<std::string> parse_model_register_request(const JsonPtr &json,
                                                        ParsedModelRegisterRequest &out,
                                                        Json::Value &details) {
  if (!json || !json->isObject()) {
    return "Body must be a JSON object";
  }

  const auto &obj = *json;
  if (!obj.isMember("path") || !obj["path"].isString()) {
    details["field"] = "path";
    return "Field 'path' is required and must be a string";
  }
  out.path = obj["path"].asString();
  if (out.path.empty()) {
    details["field"] = "path";
    return "Field 'path' cannot be empty";
  }

  if (obj.isMember("display_name")) {
    if (!obj["display_name"].isString()) {
      details["field"] = "display_name";
      return "Field 'display_name' must be a string";
    }
    const auto value = obj["display_name"].asString();
    if (!value.empty()) {
      out.display_name = value;
    }
  }

  return std::nullopt;
}

std::optional<std::string> parse_model_select_request(const JsonPtr &json,
                                                      std::string &model_id,
                                                      std::optional<int> &context_size,
                                                      Json::Value &details) {
  if (!json || !json->isObject()) {
    return "Body must be a JSON object";
  }

  const auto &obj = *json;
  if (!obj.isMember("model_id") || !obj["model_id"].isString()) {
    details["field"] = "model_id";
    return "Field 'model_id' is required and must be a string";
  }

  model_id = obj["model_id"].asString();
  if (model_id.empty()) {
    details["field"] = "model_id";
    return "Field 'model_id' cannot be empty";
  }

  if (obj.isMember("context_size")) {
    if (!obj["context_size"].isInt()) {
      details["field"] = "context_size";
      return "Field 'context_size' must be an integer";
    }
    int val = obj["context_size"].asInt();
    if (val <= 0) {
      details["field"] = "context_size";
      return "Field 'context_size' must be positive";
    }
    context_size = val;
  }

  return std::nullopt;
}

std::optional<std::string> parse_chat_complete_request(const JsonPtr &json,
                                                       std::string &message,
                                                       Json::Value &details) {
  if (!json || !json->isObject()) {
    return "Body must be a JSON object";
  }

  const auto &obj = *json;
  if (!obj.isMember("message") || !obj["message"].isString()) {
    details["field"] = "message";
    return "Field 'message' is required and must be a string";
  }

  message = obj["message"].asString();
  if (message.empty()) {
    details["field"] = "message";
    return "Field 'message' cannot be empty";
  }

  return std::nullopt;
}
