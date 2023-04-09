//
// Created by Perfare on 2020/7/4.
//

#ifndef ZYGISK_IL2CPPDUMPER_HACK_H
#define ZYGISK_IL2CPPDUMPER_HACK_H

#include <stddef.h>
#include <jni.h>

void hack_prepare(const char *game_data_dir, void *data, size_t length);

static JavaVM *g_vm = NULL;
static JNIEnv *g_env = NULL;
char* jstringToChar(JNIEnv* env, jstring jstr);
jstring getExternalStorageDirectory();

#endif //ZYGISK_IL2CPPDUMPER_HACK_H
