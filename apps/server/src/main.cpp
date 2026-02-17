#include <drogon/drogon.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace {

std::string now_rfc3339_utc() {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const auto secs = floor<seconds>(now);
  const auto ms = duration_cast<milliseconds>(now - secs).count();

  std::time_t tt = system_clock::to_time_t(now);
  std::tm utc_tm{};
#if defined(_WIN32)
  gmtime_s(&utc_tm, &tt);
#else
  gmtime_r(&tt, &utc_tm);
#endif

  std::ostringstream os;
  os << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%S") << "." << std::setw(3)
     << std::setfill('0') << ms << "Z";
  return os.str();
}

std::string generate_correlation_id() {
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  static constexpr char chars[] = "0123456789abcdefghijklmnopqrstuvwxyz";

  std::uniform_int_distribution<int> pick(0, 35);
  std::string out = "cor_";
  out.reserve(24);
  for (int i = 0; i < 20; ++i) {
    out.push_back(chars[pick(rng)]);
  }
  return out;
}

std::string resolve_correlation_id(const drogon::HttpRequestPtr &req) {
  const auto incoming = req->getHeader("X-Correlation-Id");
  if (!incoming.empty()) {
    return incoming;
  }
  return generate_correlation_id();
}

void write_json(const drogon::HttpRequestPtr &req,
                const drogon::HttpResponsePtr &resp,
                const Json::Value &json,
                drogon::HttpStatusCode code = drogon::k200OK) {
  const auto cid = resolve_correlation_id(req);
  resp->setStatusCode(code);
  resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  resp->addHeader("X-Correlation-Id", cid);
  resp->setBody(Json::FastWriter().write(json));
}

bool starts_with(const std::string &value, const std::string &prefix) {
  return value.rfind(prefix, 0) == 0;
}

}  // namespace

int main() {
  namespace fs = std::filesystem;

  const fs::path web_root = fs::path(PETTING_ZOO_WEB_ROOT);
  const fs::path index_html = web_root / "index.html";

  drogon::app().setLogLevel(trantor::Logger::kWarn);
  drogon::app().setDocumentRoot(web_root.string());
  drogon::app().setUploadPath("uploads");

  drogon::app().registerHandler("/healthz", [](const drogon::HttpRequestPtr &req,
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

  drogon::app().registerHandler(
      "/{path:.*}",
      [index_html](const drogon::HttpRequestPtr &req,
                   std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                   const std::string &path) {
        if (starts_with(path, "api/") || path == "api") {
          Json::Value error(Json::objectValue);
          Json::Value payload(Json::objectValue);
          payload["code"] = "APP-NOT-IMPL-001";
          payload["category"] = "internal";
          payload["message"] = "API endpoint not implemented in Phase 1 skeleton";
          payload["retryable"] = false;
          payload["correlation_id"] = resolve_correlation_id(req);
          error["error"] = payload;

          auto resp = drogon::HttpResponse::newHttpResponse();
          write_json(req, resp, error, drogon::k404NotFound);
          cb(resp);
          return;
        }

        if (index_html.empty() || !fs::exists(index_html)) {
          Json::Value error(Json::objectValue);
          Json::Value payload(Json::objectValue);
          payload["code"] = "APP-INT-001";
          payload["category"] = "internal";
          payload["message"] = "Web assets missing. Build apps/web to produce dist output.";
          payload["retryable"] = false;
          payload["correlation_id"] = resolve_correlation_id(req);
          error["error"] = payload;

          auto resp = drogon::HttpResponse::newHttpResponse();
          write_json(req, resp, error, drogon::k500InternalServerError);
          cb(resp);
          return;
        }

        auto resp = drogon::HttpResponse::newFileResponse(index_html.string());
        resp->setContentTypeCode(drogon::CT_TEXT_HTML);
        resp->addHeader("Cache-Control", "no-store");
        resp->addHeader("X-Correlation-Id", resolve_correlation_id(req));
        cb(resp);
      },
      {drogon::Get});

  drogon::app().addListener("127.0.0.1", 8080);
  drogon::app().run();
  return 0;
}
