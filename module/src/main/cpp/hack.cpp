//
// Created by Perfare on 2020/7/4.
//

#include "hack.h"
#include "il2cpp_dump.h"
#include "log.h"
#include "xdl.h"
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <sys/system_properties.h>
#include <dlfcn.h>
#include <jni.h>
#include <thread>
#include <sys/mman.h>
#include <linux/unistd.h>
#include <array>

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


static std::string GetNativeBridgeLibrary() {
    auto value = std::array<char, PROP_VALUE_MAX>();
    __system_property_get("ro.dalvik.vm.native.bridge", value.data());
    return {value.data()};
}

void hack_start(const char *game_data_dir) {
    bool load = false;
    for (int i = 0; i < 10; i++) {
        void *handle = xdl_open("libil2cpp.so", 0);
        if (handle) {
            load = true;
            il2cpp_api_init(handle);
            // il2cpp_dump(game_data_dir);
            hack_lua();
            break;
        } else {
            sleep(1);
        }
    }
    if (!load) {
        LOGI("libil2cpp.so not found in thread %d", gettid());
    }
}

struct NativeBridgeCallbacks {
    uint32_t version;
    void *initialize;

    void *(*loadLibrary)(const char *libpath, int flag);

    void *(*getTrampoline)(void *handle, const char *name, const char *shorty, uint32_t len);

    void *isSupported;
    void *getAppEnv;
    void *isCompatibleWith;
    void *getSignalHandler;
    void *unloadLibrary;
    void *getError;
    void *isPathSupported;
    void *initAnonymousNamespace;
    void *createNamespace;
    void *linkNamespaces;

    void *(*loadLibraryExt)(const char *libpath, int flag, void *ns);
};

void hack_prepare(const char *game_data_dir, void *data, size_t length) {
    LOGI("hack thread: %d", gettid());
    int api_level = android_get_device_api_level();
    LOGI("api level: %d", api_level);

#if defined(__i386__) || defined(__x86_64__)
    //TODO 等待houdini初始化
    sleep(5);

    auto nb = dlopen("libhoudini.so", RTLD_NOW);
    if (!nb) {
        auto native_bridge = GetNativeBridgeLibrary();
        LOGI("native bridge: %s", native_bridge.data());
        nb = dlopen(native_bridge.data(), RTLD_NOW);
    }
    if (nb) {
        LOGI("nb %p", nb);
        auto callbacks = (NativeBridgeCallbacks *) dlsym(nb, "NativeBridgeItf");
        if (callbacks) {
            LOGI("NativeBridgeLoadLibrary %p", callbacks->loadLibrary);
            LOGI("NativeBridgeLoadLibraryExt %p", callbacks->loadLibraryExt);
            LOGI("NativeBridgeGetTrampoline %p", callbacks->getTrampoline);
            auto libart = dlopen("libart.so", RTLD_NOW);
            auto JNI_GetCreatedJavaVMs = (jint (*)(JavaVM **, jsize, jsize *)) dlsym(libart,
                                                                                     "JNI_GetCreatedJavaVMs");
            LOGI("JNI_GetCreatedJavaVMs %p", JNI_GetCreatedJavaVMs);

            int fd = syscall(__NR_memfd_create, "anon", MFD_CLOEXEC);
            ftruncate(fd, (off_t) length);
            void *mem = mmap(nullptr, length, PROT_WRITE, MAP_SHARED, fd, 0);
            memcpy(mem, data, length);
            munmap(mem, length);
            munmap(data, length);
            char path[PATH_MAX];
            snprintf(path, PATH_MAX, "/proc/self/fd/%d", fd);
            LOGI("arm path %s", path);

            void *arm_handle;
            if (api_level >= 26) {
                arm_handle = callbacks->loadLibraryExt(path, RTLD_NOW, (void *) 3);
            } else {
                arm_handle = callbacks->loadLibrary(path, RTLD_NOW);
            }
            if (arm_handle) {
                LOGI("arm handle %p", arm_handle);
                JavaVM *vms_buf[1];
                jsize num_vms;
                jint status = JNI_GetCreatedJavaVMs(vms_buf, 1, &num_vms);
                if (status == JNI_OK && num_vms > 0) {
                    auto init = (void (*)(JavaVM *, void *)) callbacks->getTrampoline(arm_handle,
                                                                                      "JNI_OnLoad",
                                                                                      nullptr, 0);
                    LOGI("JNI_OnLoad %p", init);
                    init(vms_buf[0], (void *) game_data_dir);
                }
            }
            close(fd);
        }
    } else {
#endif
        hack_start(game_data_dir);
#if defined(__i386__) || defined(__x86_64__)
    }
#endif
}

#if defined(__arm__) || defined(__aarch64__)

static JavaVM *g_vm = NULL;
static JNIEnv *g_env = NULL;
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    // 设置g_env
    vm->GetEnv((void **)&g_env, JNI_VERSION_1_6);

    auto game_data_dir = (const char *) reserved;
    std::thread hack_thread(hack_start, game_data_dir);
    hack_thread.detach();
    return JNI_VERSION_1_6;
}

char* jstringToChar(JNIEnv* env, jstring jstr) {
    const char* utf_chars = env->GetStringUTFChars(jstr, NULL);
    char* chars = new char[strlen(utf_chars) + 1];
    strcpy(chars, utf_chars);
    env->ReleaseStringUTFChars(jstr, utf_chars);
    return chars;
}
jstring getExternalStorageDirectory() {
    jclass clazz = g_env->FindClass("android/os/Environment");
    jmethodID getExternalStorageDirectory = g_env->GetStaticMethodID(clazz, "getExternalStorageDirectory", "()Ljava/io/File;");
    jobject file_obj = g_env->CallStaticObjectMethod(clazz, getExternalStorageDirectory);

    jclass file_clazz = g_env->GetObjectClass(file_obj);
    jmethodID getPath = g_env->GetMethodID(file_clazz, "getPath", "()Ljava/lang/String;");
    jstring path_obj = (jstring)g_env->CallObjectMethod(file_obj, getPath);

    return path_obj;
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

#endif