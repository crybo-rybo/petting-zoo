#include "routes.hpp"

#include <drogon/drogon.h>

#include "http_helpers.hpp"

namespace {

void handle_deferred(const drogon::HttpRequestPtr &req,
                     std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
  write_error(req, std::move(cb), drogon::k404NotFound, "APP-NOT-IMPL-001",
              "internal",
              "API endpoint not implemented in MVP reset scope",
              false);
}

}  // namespace

void register_deferred_routes() {
  drogon::app().registerHandler(
      "/api/sessions",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        handle_deferred(req, std::move(cb));
      },
      {drogon::Get, drogon::Post});

  drogon::app().registerHandler(
      "/api/sessions/{1}",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb,
         const std::string &) { handle_deferred(req, std::move(cb)); },
      {drogon::Delete});

  drogon::app().registerHandler(
      "/api/chat/{1}/send",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb,
         const std::string &) { handle_deferred(req, std::move(cb)); },
      {drogon::Post});

  drogon::app().registerHandler(
      "/api/chat/{1}/stream",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb,
         const std::string &) { handle_deferred(req, std::move(cb)); },
      {drogon::Get});

  drogon::app().registerHandler(
      "/api/kb/upload",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        handle_deferred(req, std::move(cb));
      },
      {drogon::Post});

  drogon::app().registerHandler(
      "/api/kb/docs",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        handle_deferred(req, std::move(cb));
      },
      {drogon::Get});

  drogon::app().registerHandler(
      "/api/kb/docs/{1}",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb,
         const std::string &) { handle_deferred(req, std::move(cb)); },
      {drogon::Delete});

  drogon::app().registerHandler(
      "/api/prompts/{1}",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb,
         const std::string &) { handle_deferred(req, std::move(cb)); },
      {drogon::Get, drogon::Put});
}
