#pragma once

#include <json/json.h>

#include "runtime_state.hpp"

Json::Value model_to_json(const ModelEntry &model);
