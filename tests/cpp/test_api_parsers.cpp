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
  
  auto err = parse_chat_complete_request(json_ptr, message, details, 100);
  assert(!err.has_value());
  assert(message == "hello");
}

void test_parse_chat_complete_request_missing_message() {
  Json::Value req(Json::objectValue);
  
  auto json_ptr = std::make_shared<Json::Value>(req);
  std::string message;
  Json::Value details;
  
  auto err = parse_chat_complete_request(json_ptr, message, details, 100);
  assert(err.has_value());
  assert(details["field"].asString() == "message");
}

void test_parse_chat_complete_request_empty_message() {
  Json::Value req(Json::objectValue);
  req["message"] = "";
  
  auto json_ptr = std::make_shared<Json::Value>(req);
  std::string message;
  Json::Value details;
  
  auto err = parse_chat_complete_request(json_ptr, message, details, 100);
  assert(err.has_value());
  assert(details["field"].asString() == "message");
}

void test_parse_chat_complete_request_too_long() {
  Json::Value req(Json::objectValue);
  req["message"] = "hello world";

  auto json_ptr = std::make_shared<Json::Value>(req);
  std::string message;
  Json::Value details;

  auto err = parse_chat_complete_request(json_ptr, message, details, 5);
  assert(err.has_value());
  assert(details["field"].asString() == "message");
  assert(details["max_chars"].asUInt64() == 5);
}

int main() {
  test_parse_chat_complete_request_valid();
  test_parse_chat_complete_request_missing_message();
  test_parse_chat_complete_request_empty_message();
  test_parse_chat_complete_request_too_long();
  std::cout << "All parse tests passed!" << std::endl;
  return 0;
}
