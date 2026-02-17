#include <iostream>

#include <zoo/types.hpp>

int main() {
  {
    zoo::Config cfg;
    auto result = cfg.validate();
    if (result) {
      std::cerr << "expected empty config to fail validation\n";
      return 1;
    }
  }

  {
    zoo::Config cfg;
    cfg.model_path = "/tmp/model.gguf";
    cfg.context_size = 0;
    auto result = cfg.validate();
    if (result) {
      std::cerr << "expected non-positive context size to fail validation\n";
      return 1;
    }
  }

  {
    zoo::Config cfg;
    cfg.model_path = "/tmp/model.gguf";
    cfg.context_size = 2048;
    cfg.max_tokens = 256;
    auto result = cfg.validate();
    if (!result) {
      std::cerr << "expected valid config to pass validation: "
                << result.error().to_string() << "\n";
      return 1;
    }
  }

  return 0;
}
