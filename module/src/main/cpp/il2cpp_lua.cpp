//
// Created by Perfare on 2020/7/4.
//

#include "il2cpp_lua.h"
#include "hack.h"
#include <dlfcn.h>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include "xdl.h"
#include "log.h"
#include "il2cpp-tabledefs.h"
#include "il2cpp-class.h"

#define DO_API(r, n, p) r (*n) p

#include "il2cpp-api-functions.h"

#undef DO_API

static uint64_t il2cpp_base = 0;

void init_il2cpp_api(void *handle) {
#define DO_API(r, n, p) {                      \
    n = (r (*) p)xdl_sym(handle, #n, nullptr); \
    if(!n) {                                   \
        LOGW("api not found %s", #n);          \
    }                                          \
}

#include "il2cpp-api-functions.h"

#undef DO_API
}

void il2cpp_api_init(void *handle) {
    LOGI("il2cpp_handle: %p", handle);
    init_il2cpp_api(handle);
    if (il2cpp_domain_get_assemblies) {
        Dl_info dlInfo;
        if (dladdr((void *) il2cpp_domain_get_assemblies, &dlInfo)) {
            il2cpp_base = reinterpret_cast<uint64_t>(dlInfo.dli_fbase);
        }
        LOGI("il2cpp_base: %" PRIx64"", il2cpp_base);
    } else {
        LOGE("Failed to initialize il2cpp api.");
        return;
    }
    while (!il2cpp_is_vm_thread(nullptr)) {
        LOGI("Waiting for il2cpp_init...");
        sleep(1);
    }
    auto domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);
}


void hack_lua() {
    LOGI("start hack lua");

    // 初始化需要用到的dll
    const Il2CppImage* game;
    const Il2CppImage* corlib;
    const Il2CppImage* unity_core;

    size_t size;
    auto domain = il2cpp_domain_get();
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);
    for (int i = 0; i < size; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        auto name = il2cpp_image_get_name(image);
        LOGI("image name: %s", name);
        if (strcmp(name, "Assembly-CSharp.dll") == 0)
        {
            game = image;
            LOGI("image match: %s", name);
        }
        else if (strcmp(name, "mscorlib.dll") == 0)
        {
            corlib = image;
            LOGI("image match: %s", name);
        }
        else if (strcmp(name, "UnityEngine.CoreModule.dll") == 0)
        {
            unity_core = image;
            LOGI("image match: %s", name);
        }
    }

    // 初始化需要用到的函数
    auto application = il2cpp_class_from_name(unity_core, "UnityEngine", "Application");
    auto get_persistentDataPath = il2cpp_class_get_method_from_name(application, "get_persistentDataPath", 0);

    jstring path_jstr = getExternalStorageDirectory();
    char* path_chars = jstringToChar(g_env, path_jstr);
    LOGI("path : %s", path_chars);
    delete[] path_chars;
}

