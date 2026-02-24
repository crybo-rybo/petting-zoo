#include "api_serialization.hpp"

Json::Value model_to_json(const ModelEntry &model) {
  Json::Value out(Json::objectValue);
  out["id"] = model.id;
  out["display_name"] = model.display_name;
  out["path"] = model.path;
  out["status"] = model.status;
  out["context_size"] = model.context_size;
  out["file_size_bytes"] = static_cast<Json::UInt64>(model.file_size_bytes);
  return out;
}
