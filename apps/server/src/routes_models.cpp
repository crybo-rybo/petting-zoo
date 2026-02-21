#include "routes.hpp"

#include <drogon/drogon.h>

#include "api_parsers.hpp"
#include "api_serialization.hpp"
#include "http_helpers.hpp"

void register_model_routes(RuntimeState &runtime_state) {
  drogon::app().registerHandler(
      "/api/models",
      [&runtime_state](const drogon::HttpRequestPtr &req,
                       std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        Json::Value body(Json::objectValue);
        Json::Value models(Json::arrayValue);
        for (const auto &model : runtime_state.list_models()) {
          models.append(model_to_json(model));
        }
        body["models"] = models;
        if (const auto active = runtime_state.active_model_id(); active.has_value()) {
          body["active_model_id"] = *active;
        } else {
          body["active_model_id"] = Json::nullValue;
        }

        auto resp = drogon::HttpResponse::newHttpResponse();
        write_json(req, resp, body);
        cb(resp);
      },
      {drogon::Get});

  drogon::app().registerHandler(
      "/api/models/register",
      [&runtime_state](const drogon::HttpRequestPtr &req,
                       std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        ParsedModelRegisterRequest parsed;
        Json::Value details(Json::objectValue);
        if (const auto parse_error =
                parse_model_register_request(req->getJsonObject(), parsed, details);
            parse_error.has_value()) {
          write_error(req, std::move(cb), drogon::k400BadRequest, "APP-VAL-001",
                      "validation", *parse_error, false, details);
          return;
        }

        std::string error_code;
        std::string error_message;
        const auto model = runtime_state.register_model(parsed, error_code, error_message);
        if (!model.has_value()) {
          write_error(req, std::move(cb), drogon::k400BadRequest, error_code,
                      "validation", error_message, false);
          return;
        }

        Json::Value body(Json::objectValue);
        body["model"] = model_to_json(*model);
        auto resp = drogon::HttpResponse::newHttpResponse();
        write_json(req, resp, body, drogon::k201Created);
        cb(resp);
      },
      {drogon::Post});

  drogon::app().registerHandler(
      "/api/models/select",
      [&runtime_state](const drogon::HttpRequestPtr &req,
                       std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        std::string model_id;
        Json::Value details(Json::objectValue);
        if (const auto parse_error =
                parse_model_select_request(req->getJsonObject(), model_id, details);
            parse_error.has_value()) {
          write_error(req, std::move(cb), drogon::k400BadRequest, "APP-VAL-001",
                      "validation", *parse_error, false, details);
          return;
        }

        std::string error_code;
        std::string error_message;
        const auto model = runtime_state.select_model(model_id, error_code, error_message);
        if (!model.has_value()) {
          auto status = drogon::k409Conflict;
          auto category = std::string("conflict");
          if (error_code == "APP-MOD-404") {
            status = drogon::k404NotFound;
            category = "not_found";
          } else if (error_code == "APP-VAL-001") {
            status = drogon::k400BadRequest;
            category = "validation";
          }
          write_error(req, std::move(cb), status, error_code, category, error_message,
                      true);
          return;
        }

        Json::Value body(Json::objectValue);
        body["active_model"] = model_to_json(*model);
        auto resp = drogon::HttpResponse::newHttpResponse();
        write_json(req, resp, body);
        cb(resp);
      },
      {drogon::Post});
}
