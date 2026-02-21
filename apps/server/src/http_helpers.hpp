#pragma once

#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>

#include <optional>
#include <string>

std::string now_rfc3339_utc();
std::string generate_correlation_id();
std::string resolve_correlation_id(const drogon::HttpRequestPtr &req);

void write_json(const drogon::HttpRequestPtr &req,
                const drogon::HttpResponsePtr &resp,
                const Json::Value &json,
                drogon::HttpStatusCode code = drogon::k200OK);

void write_error(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                 drogon::HttpStatusCode status,
                 std::string code,
                 std::string category,
                 std::string message,
                 bool retryable,
                 const std::optional<Json::Value> &details = std::nullopt);
