//
// Created by Perfare on 2020/7/4.
//

// #include <dobby.h>
#include "il2cpp_string.h"
#include "il2cpp_lua.h"
#include <jni.h>
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


const Il2CppImage* get_image(const char* image_name) {
    const Il2CppImage* result;
    size_t size;
    Il2CppDomain * domain = il2cpp_domain_get();
    const Il2CppAssembly ** assemblies = il2cpp_domain_get_assemblies(domain, &size);
    for (int i = 0; i < size; ++i) {
        const Il2CppImage * image = il2cpp_assembly_get_image(assemblies[i]);
        const char * name = il2cpp_image_get_name(image);
        if (strcmp(name, image_name) == 0)
        {
            LOGI("image match: %s", name);
            result = image;
        }
    }
    return result;
}
int32_t get_arch() {
    const Il2CppImage* corlib = get_image("mscorlib.dll");
    Il2CppClass * intPtr = il2cpp_class_from_name(corlib, "System", "IntPtr");
    const MethodInfo * get_Size = il2cpp_class_get_method_from_name(intPtr, "get_Size", 0);

    typedef int32_t (*get_Size_ftn)(void *);
    int32_t size = ((get_Size_ftn) get_Size->methodPointer)(nullptr);

    LOGI("int ptr size: %d", size);
    return size == 4 ? 32 : 64;
}
const char* get_persistentDataPath() {
    const Il2CppImage* unity_core = get_image("UnityEngine.CoreModule.dll");
    Il2CppClass * application = il2cpp_class_from_name(unity_core, "UnityEngine", "Application");
    const MethodInfo * get_persistentDataPath = il2cpp_class_get_method_from_name(application, "get_persistentDataPath", 0);

    // 定义函数类型:public static String get_persistentDataPath() { }
    typedef Il2CppString* (*get_persistentDataPath_ftn)(void *);
    // 把函数指针强转为函数类型,并调用
    Il2CppString* path = ((get_persistentDataPath_ftn) get_persistentDataPath->methodPointer)(nullptr);
    return String::GetChar((System_String_o*) path);
}
const char* get_hack_name() {
    int32_t arch = get_arch();
    if (arch == 32) {
        return "Hack32";
    } 
    else {
        return "Hack64";
    } 
}
char* get_hack_file() {
    const char* dir = get_persistentDataPath();
    const char* name = get_hack_name();
    char* path;
    if (dir[strlen(dir)-1] == '/') {  // 判断路径末尾是否有 '/'
        path = new char[strlen(dir) + strlen(name) + 1];  // 分配内存
        strcpy(path, dir);  // 复制目录路径
        strcat(path, name);  // 拼接文件名
    } else {
        path = new char[strlen(dir) + strlen(name) + 2];  // 分配内存
        strcpy(path, dir);  // 复制目录路径
        strcat(path, "/");  // 添加 '/'
        strcat(path, name);  // 拼接文件名
    }
    return path;
}

int32_t (*old_loadbuffer) (intptr_t luaState, Il2CppArray* buff, int32_t size, System_String_o* name, const MethodInfo* method);
int32_t new_loadbuffer (intptr_t luaState, Il2CppArray* buff, int32_t size, System_String_o* name, const MethodInfo* method) {
    LOGI("lua name: %s", String::GetChar(name));
    return old_loadbuffer(luaState, buff, size, name, method);
}

void hack_lua() {
    LOGI("start hack lua");

    char* hack_file = get_hack_file();
    LOGI("hack_file: %s", hack_file);

    const Il2CppImage* game = get_image("Assembly-CSharp.dll");
    Il2CppClass * luaDll = il2cpp_class_from_name(game, "LuaInterface", "LuaDLL");
    const MethodInfo * tolua_loadbuffer = il2cpp_class_get_method_from_name(luaDll, "tolua_loadbuffer", 4);

    // typedef int32_t (*tolua_loadbuffer_ftn)(intptr_t, System_Byte_array*, int32_t, System_String_o*, const MethodInfo*);
    // // 把函数指针强转为函数类型,并调用
    // Il2CppString* path = ((get_persistentDataPath_ftn) tolua_loadbuffer->methodPointer)(nullptr);
    // return String::GetChar((System_String_o*) path);

    // Namespace: LuaInterface
    // public class LuaDLL // TypeDefIndex: 5764
    // public static extern int tolua_loadbuffer(IntPtr luaState, byte[] buff, int size, string name) { }

    DobbyHook((void *)tolua_loadbuffer->methodPointer, (void*)new_loadbuffer, (void **)&old_loadbuffer);
}

