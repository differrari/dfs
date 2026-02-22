#pragma once

#include "types.h"
#include "system_module.h"

bool load_module(system_module *module);
bool unload_module(system_module *module);
system_module* get_module(char **full_path);