#ifndef ZYGISK_IL2CPPDUMPER_IL2CPP_DUMP_H
#define ZYGISK_IL2CPPDUMPER_IL2CPP_DUMP_H

#include <jni.h>

void il2cpp_api_init(void *handle);

static JavaVM *g_vm = NULL;
static JNIEnv *g_env = NULL;
char* jstringToChar(JNIEnv* env, jstring jstr);
jstring getExternalStorageDirectory();

void hack_lua();

#endif //ZYGISK_IL2CPPDUMPER_IL2CPP_DUMP_H
