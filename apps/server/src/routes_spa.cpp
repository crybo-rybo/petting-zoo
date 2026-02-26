#include "routes.hpp"

#include <filesystem>

#include <drogon/drogon.h>

#include "http_helpers.hpp"

void register_spa_routes(const std::filesystem::path &web_root,
                         const std::filesystem::path &index_html) {
  namespace fs = std::filesystem;

  drogon::app().registerHandler(
      "/{path:.*}",
      [web_root, index_html](const drogon::HttpRequestPtr &req,
                             std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                             const std::string &path) {
        const fs::path norm_root = web_root.lexically_normal();
        const fs::path requested_file = (web_root / path).lexically_normal();
        const std::string root_str = norm_root.string();
        const std::string req_str = requested_file.string();

        if (req_str.rfind(root_str, 0) != 0) {
          LOG_WARN << "Path traversal attempt blocked: " << path;
          write_error(req, std::move(cb), drogon::k403Forbidden, "APP-SEC-403",
                      "forbidden", "Forbidden path", false);
          return;
        }

        if (!path.empty() && fs::exists(requested_file) && fs::is_regular_file(requested_file)) {
          auto resp = drogon::HttpResponse::newFileResponse(requested_file.string());
          resp->addHeader("Cache-Control", "no-store");
          resp->addHeader("X-Correlation-Id", resolve_correlation_id(req));
          cb(resp);
          return;
        }

        if (!path.empty() && path.find('.') != std::string::npos) {
          write_error(req, std::move(cb), drogon::k404NotFound, "APP-ASSET-404",
                      "not_found", "Static asset not found", false);
          return;
        }

        if (index_html.empty() || !fs::exists(index_html)) {
          write_error(req, std::move(cb), drogon::k500InternalServerError,
                      "APP-INT-001", "internal",
                      "Web assets missing. Build apps/web to produce dist output.", false);
          return;
        }

        auto resp = drogon::HttpResponse::newFileResponse(index_html.string());
        resp->setContentTypeCode(drogon::CT_TEXT_HTML);
        resp->addHeader("Cache-Control", "no-store");
        resp->addHeader("X-Correlation-Id", resolve_correlation_id(req));
        cb(resp);
      },
      {drogon::Get});
}
