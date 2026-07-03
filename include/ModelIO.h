#pragma once

#include "Models.h"

#include <string>

void save_model(const ModelPack& pack, const std::string& path);
ModelPack load_model(const std::string& path);
