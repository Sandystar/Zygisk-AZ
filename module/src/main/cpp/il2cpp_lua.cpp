//
// Created by Perfare on 2020/7/4.
//

#include "dobby.h"
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
    int32_t size = ((get_Size_ftn)get_Size->methodPointer)(nullptr);

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
    Il2CppString* path = ((get_persistentDataPath_ftn)get_persistentDataPath->methodPointer)(nullptr);
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
void do_hack_file() {
    char* hack_file = get_hack_file();
    Il2CppString* hack_file_il2cpp_str = il2cpp_string_new(hack_file);
    LOGI("hack file: %s", hack_file);

    const Il2CppImage* corlib = il2cpp_get_corlib();
    Il2CppClass* file = il2cpp_class_from_name(corlib, "System.IO", "File");
    const MethodInfo* file_exists = il2cpp_class_get_method_from_name(file, "Exists", 1);
    const MethodInfo* file_readbytes = il2cpp_class_get_method_from_name(file, "ReadAllBytes", 1);

    typedef bool (*file_exists_ftn)(Il2CppString*, void *);
    bool isExist = ((file_exists_ftn)file_exists->methodPointer)(hack_file_il2cpp_str, nullptr);
    if (isExist) {
        LOGI("hack file exist");

        // hack文件数据
        typedef Il2CppArray* (*file_readbytes_ftn)(Il2CppString*, void *);
        Il2CppArray* buffer = ((file_readbytes_ftn)file_readbytes->methodPointer)(hack_file_il2cpp_str, nullptr);
        LOGI("hack file read succ: %p", buffer);

        const Il2CppImage* game = get_image("Assembly-CSharp.dll");
        // LuaScriptsMgr 类
        Il2CppClass* lua_mgr = il2cpp_class_from_name(game, "", "LuaScriptMgr");
        // LuaScriptsMgr.luaState 字段
        FieldInfo* luastate = il2cpp_class_get_field_from_name(lua_mgr, "luaState");
        // LuaScriptsMgr.get_Inst() 函数
        const MethodInfo* get_Inst = il2cpp_class_get_method_from_name(lua_mgr, "get_Inst", 0);
        // 调用LuaScriptsMgr.get_Inst()函数,获取LuaScriptsMgr的实例
        typedef Il2CppObject* (*get_Inst_ftn)(void *);
        Il2CppObject* lua_mgr_ins = ((get_Inst_ftn)get_Inst->methodPointer)(nullptr);
        // 通过LuaScriptsMgr的实例,获取字段luastate的值(LuaState类的实例)
        Il2CppObject* luastate_ins = il2cpp_field_get_value_object(luastate, lua_mgr_ins);
        LOGI("lua_mgr_ins: %p", lua_mgr_ins);
        LOGI("luastate_ins: %p", luastate_ins);

        // LuaState 类
        Il2CppClass* luastate_cls = il2cpp_class_from_name(game, "LuaInterface", "LuaState");
        // LuaState.LuaLoadBuffer() 函数
        const MethodInfo* loadbuffer = il2cpp_class_get_method_from_name(luastate_cls, "LuaLoadBuffer", 2);
        // 调用LuaState.LuaLoadBuffer()函数
        typedef void (*loadbuffer_ftn)(Il2CppObject*, Il2CppArray*, Il2CppString*, void *);
        ((loadbuffer_ftn)loadbuffer->methodPointer)(luastate_ins, buffer, hack_file_il2cpp_str, nullptr);
    }
    else {
        LOGI("hack_file not exist");
    }
}

// hook lua的启动函数
void (*old_start_lua) (Il2CppObject* __this, const MethodInfo* method);
void new_start_lua (Il2CppObject* __this, const MethodInfo* method) {
    LOGI("start game lua");
    old_start_lua(__this, method);

    // 启动完成后运行自己的hack
    LOGI("start do hack");
    do_hack_file();
}
void hook_start_lua() {
    const Il2CppImage* game = get_image("Assembly-CSharp.dll");
    Il2CppClass * lua_mgr = il2cpp_class_from_name(game, "", "LuaScriptMgr");
    const MethodInfo * start_lua = il2cpp_class_get_method_from_name(lua_mgr, "StartLua", 0);

    DobbyHook((void *)start_lua->methodPointer, (void*)new_start_lua, (void **)&old_start_lua);
}

void hack_lua() {
    LOGI("start hack");
    hook_start_lua();    
}

// // hook lua的加载函数
// int32_t (*old_loadbuffer) (intptr_t luaState, Il2CppArray* buff, int32_t size, System_String_o* name, const MethodInfo* method);
// int32_t new_loadbuffer (intptr_t luaState, Il2CppArray* buff, int32_t size, System_String_o* name, const MethodInfo* method) {
//     int32_t result = old_loadbuffer(luaState, buff, size, name, method);
    
//     const char* chunk_name = String::GetChar(name);
//     if (strcmp(chunk_name, "@main.lua") == 0)
//     {
//         LOGI("lua match: %s", chunk_name);
//         // 执行Hack
//         do_hack_file();
//     }
//     return result;
// }
// void hook_lua_load() {
//     const Il2CppImage* game = get_image("Assembly-CSharp.dll");
//     Il2CppClass * luaDll = il2cpp_class_from_name(game, "LuaInterface", "LuaDLL");
//     const MethodInfo * tolua_loadbuffer = il2cpp_class_get_method_from_name(luaDll, "tolua_loadbuffer", 4);

//     DobbyHook((void *)tolua_loadbuffer->methodPointer, (void*)new_loadbuffer, (void **)&old_loadbuffer);
// }
