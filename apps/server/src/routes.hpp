#pragma once

#include <filesystem>

#include "runtime_state.hpp"

void register_health_routes();
void register_model_routes(RuntimeState &runtime_state);
void register_chat_routes(RuntimeState &runtime_state);
void register_deferred_routes();
void register_spa_routes(const std::filesystem::path &web_root,
                         const std::filesystem::path &index_html);
