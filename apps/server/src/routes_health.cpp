#include "routes.hpp"

#include <drogon/drogon.h>

#include "http_helpers.hpp"

void register_health_routes() {
  drogon::app().registerHandler(
      "/healthz",
      [](const drogon::HttpRequestPtr &req,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        Json::Value body(Json::objectValue);
        body["status"] = "ok";
        body["service"] = "petting-zoo-server";
        body["version"] = PETTING_ZOO_VERSION;
        body["timestamp"] = now_rfc3339_utc();

        auto resp = drogon::HttpResponse::newHttpResponse();
        write_json(req, resp, body, drogon::k200OK);
        cb(resp);
      });
}
