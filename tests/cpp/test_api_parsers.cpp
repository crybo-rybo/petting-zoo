#include "../../apps/server/src/api_parsers.hpp"
#include <json/json.h>
#include <iostream>
#include <cassert>
#include <memory>

void test_parse_chat_complete_request_valid() {
  Json::Value req(Json::objectValue);
  req["message"] = "hello";
  
  auto json_ptr = std::make_shared<Json::Value>(req);
  std::string message;
  Json::Value details;
  
  auto err = parse_chat_complete_request(json_ptr, message, details);
  assert(!err.has_value());
  assert(message == "hello");
}

void test_parse_chat_complete_request_missing_message() {
  Json::Value req(Json::objectValue);
  
  auto json_ptr = std::make_shared<Json::Value>(req);
  std::string message;
  Json::Value details;
  
  auto err = parse_chat_complete_request(json_ptr, message, details);
  assert(err.has_value());
  assert(details["field"].asString() == "message");
}

void test_parse_chat_complete_request_empty_message() {
  Json::Value req(Json::objectValue);
  req["message"] = "";
  
  auto json_ptr = std::make_shared<Json::Value>(req);
  std::string message;
  Json::Value details;
  
  auto err = parse_chat_complete_request(json_ptr, message, details);
  assert(err.has_value());
  assert(details["field"].asString() == "message");
}

int main() {
  test_parse_chat_complete_request_valid();
  test_parse_chat_complete_request_missing_message();
  test_parse_chat_complete_request_empty_message();
  std::cout << "All parse tests passed!" << std::endl;
  return 0;
}
