#include "http_helpers.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>

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
                drogon::HttpStatusCode code) {
  const auto cid = resolve_correlation_id(req);
  resp->setStatusCode(code);
  resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  resp->addHeader("X-Correlation-Id", cid);
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  resp->setBody(Json::writeString(builder, json));
}

void write_error(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                 drogon::HttpStatusCode status,
                 std::string code,
                 std::string category,
                 std::string message,
                 bool retryable,
                 const std::optional<Json::Value> &details) {
  Json::Value payload(Json::objectValue);
  payload["code"] = std::move(code);
  payload["category"] = std::move(category);
  payload["message"] = std::move(message);
  payload["retryable"] = retryable;
  payload["correlation_id"] = resolve_correlation_id(req);
  if (details.has_value()) {
    payload["details"] = *details;
  }

  Json::Value error(Json::objectValue);
  error["error"] = payload;

  auto resp = drogon::HttpResponse::newHttpResponse();
  write_json(req, resp, error, status);
  cb(resp);
}
