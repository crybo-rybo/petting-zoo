#include "routes.hpp"

#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>

#include <thread>

#include "api_parsers.hpp"
#include "http_helpers.hpp"

void register_chat_routes(RuntimeState &runtime_state) {
  drogon::app().registerHandler(
      "/api/chat/complete",
      [&runtime_state](const drogon::HttpRequestPtr &req,
                       std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        std::string message;
        Json::Value details(Json::objectValue);
        if (const auto parse_error =
                parse_chat_complete_request(req->getJsonObject(), message, details);
            parse_error.has_value()) {
          write_error(req, std::move(cb), drogon::k400BadRequest, "APP-VAL-001",
                      "validation", *parse_error, false, details);
          return;
        }

        std::string error_code;
        std::string error_message;
        const auto response = runtime_state.chat_complete(message, error_code, error_message);
        if (!response.has_value()) {
          const auto status = error_code == "APP-STATE-409" ? drogon::k409Conflict
                                                              : drogon::k502BadGateway;
          write_error(req, std::move(cb), status, error_code,
                      status == drogon::k409Conflict ? "conflict" : "upstream",
                      error_message, true);
          return;
        }

        Json::Value usage(Json::objectValue);
        usage["prompt_tokens"] = response->usage.prompt_tokens;
        usage["completion_tokens"] = response->usage.completion_tokens;
        usage["total_tokens"] = response->usage.total_tokens;

        Json::Value metrics(Json::objectValue);
        metrics["latency_ms"] = static_cast<Json::Int64>(response->metrics.latency_ms.count());
        metrics["time_to_first_token_ms"] =
            static_cast<Json::Int64>(response->metrics.time_to_first_token_ms.count());
        metrics["tokens_per_second"] = response->metrics.tokens_per_second;

        Json::Value body(Json::objectValue);
        body["text"] = response->text;
        body["usage"] = usage;
        body["metrics"] = metrics;

        auto resp = drogon::HttpResponse::newHttpResponse();
        write_json(req, resp, body);
        cb(resp);
      },
      {drogon::Post});

  drogon::app().registerHandler(
      "/api/chat/stream",
      [&runtime_state](const drogon::HttpRequestPtr &req,
                       std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        std::string message;
        Json::Value details(Json::objectValue);
        if (const auto parse_error =
                parse_chat_complete_request(req->getJsonObject(), message, details);
            parse_error.has_value()) {
          write_error(req, std::move(cb), drogon::k400BadRequest, "APP-VAL-001",
                      "validation", *parse_error, false, details);
          return;
        }

        const auto cid = resolve_correlation_id(req);

        auto resp = drogon::HttpResponse::newAsyncStreamResponse(
            [&runtime_state, message = std::move(message)](
                drogon::ResponseStreamPtr stream) mutable {
              // Move the unique_ptr into shared ownership so the inference thread
              // and token callback can safely call send() without holding the
              // unique_ptr exclusively.
              auto ss = std::shared_ptr<drogon::ResponseStream>(std::move(stream));

              std::thread([&runtime_state, msg = std::move(message),
                           ss = std::move(ss)]() mutable {
                auto token_cb = [&ss](std::string_view token) {
                  Json::Value event(Json::objectValue);
                  event["type"] = "token";
                  event["content"] = std::string(token);
                  Json::StreamWriterBuilder builder;
                  builder["indentation"] = "";
                  ss->send("data: " + Json::writeString(builder, event) + "\n\n");
                };

                std::string error_code;
                std::string error_message;
                const auto result = runtime_state.chat_stream(
                    msg, std::move(token_cb), error_code, error_message);

                if (!result) {
                  Json::Value err(Json::objectValue);
                  err["type"] = "error";
                  err["code"] = error_code;
                  err["message"] = error_message;
                  Json::StreamWriterBuilder builder;
                  builder["indentation"] = "";
                  ss->send("data: " + Json::writeString(builder, err) + "\n\n");
                  ss->close();
                  return;
                }

                Json::Value usage(Json::objectValue);
                usage["prompt_tokens"] = result->usage.prompt_tokens;
                usage["completion_tokens"] = result->usage.completion_tokens;
                usage["total_tokens"] = result->usage.total_tokens;

                Json::Value metrics(Json::objectValue);
                metrics["latency_ms"] =
                    static_cast<Json::Int64>(result->metrics.latency_ms.count());
                metrics["time_to_first_token_ms"] = static_cast<Json::Int64>(
                    result->metrics.time_to_first_token_ms.count());
                metrics["tokens_per_second"] = result->metrics.tokens_per_second;

                Json::Value done(Json::objectValue);
                done["type"] = "done";
                done["text"] = result->text;
                done["usage"] = usage;
                done["metrics"] = metrics;

                Json::StreamWriterBuilder builder;
                builder["indentation"] = "";
                ss->send("data: " + Json::writeString(builder, done) + "\n\n");
                ss->close();
              }).detach();
            },
            /*disableKickoffTimeout=*/true);

        resp->setContentTypeString("text/event-stream");
        resp->addHeader("Cache-Control", "no-cache");
        resp->addHeader("X-Accel-Buffering", "no");
        resp->addHeader("X-Correlation-Id", cid);
        cb(resp);
      },
      {drogon::Post});

  drogon::app().registerHandler(
      "/api/chat/reset",
      [&runtime_state](const drogon::HttpRequestPtr &req,
                       std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        std::string error_code;
        std::string error_message;
        const auto model_id = runtime_state.reset_chat(error_code, error_message);
        if (!model_id.has_value()) {
          write_error(req, std::move(cb), drogon::k409Conflict, error_code, "conflict",
                      error_message, false);
          return;
        }

        Json::Value body(Json::objectValue);
        body["status"] = "cleared";
        body["model_id"] = *model_id;
        auto resp = drogon::HttpResponse::newHttpResponse();
        write_json(req, resp, body);
        cb(resp);
      },
      {drogon::Post});

  drogon::app().registerHandler(
      "/api/chat/clear_memory",
      [&runtime_state](const drogon::HttpRequestPtr &req,
                       std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        std::string error_code;
        std::string error_message;
        const auto model_id = runtime_state.clear_memory(error_code, error_message);
        if (!model_id.has_value()) {
          auto status = error_code == "APP-STATE-500" ? drogon::k500InternalServerError : drogon::k502BadGateway;
          write_error(req, std::move(cb), status, error_code, "server_error",
                      error_message, false);
          return;
        }

        Json::Value body(Json::objectValue);
        body["status"] = "memory_wiped";
        body["model_id"] = *model_id;
        auto resp = drogon::HttpResponse::newHttpResponse();
        write_json(req, resp, body);
        cb(resp);
      },
      {drogon::Post});
}
