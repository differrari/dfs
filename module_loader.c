#include "module_loader.h"
#include "data/struct/hashmap.h"
#include "string/string.h"
#include "syscalls/syscalls.h"

hash_map_t *module_map;

u64 hash_module(const char *name, size_t len){
    return hash_map_fnv1a64(name, len ? len : strlen(name));
}

bool load_module(system_module *module){
    if (!module_map){
        module_map = hash_map_create(64);
    }
    if (!module) return false;
    if (!module->init){
        print("[FS_MOD error] failed to load module %s due to missing initializer",module->name);
        return false;
    }
    if (!module->init()){
        print("[FS_MOD error] failed to load module %s. Initializer failed",module->name);
        return false;
    }
    u64 hash = hash_module(module->mount, 0);
    if (hash_map_get(module_map, &hash, sizeof(u64))){
        print("[FS_MOD error] failed to load module %s. Module already exists (or has collision, my bad)",module->name);
        return false;
    }
    hash_map_put(module_map, &hash, sizeof(u64), module);
    return true;
}

bool unload_module(system_module *module){
    if (!module_map) return false;
    if (!module) return false;
    if (!module->init) return false;
    if (module->fini) module->fini();
    u64 hash = hash_module(module->mount, 0);
    void *mod;
    return hash_map_remove(module_map, &hash, sizeof(u64), &mod);
}

system_module* get_module(char **full_path){
    if (!module_map) return 0;
    if (!full_path || !*full_path) return 0;
    if (**full_path == '/') *full_path = *full_path + 1;
    char *next = (char*)seek_to(*full_path, '/');
    
    if (next == *full_path) return 0;
    int offset = *(next-1) == '/';
    
    u64 hash = hash_module(*full_path,next-*full_path-offset);
    
    system_module *mod = hash_map_get(module_map, &hash, sizeof(u64));
    
    *full_path = next;
    return mod;
}
