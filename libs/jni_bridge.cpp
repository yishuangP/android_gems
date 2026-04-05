/**
 * JNI bridge for stable-diffusion.cpp with progress callback.
 */
#include <jni.h>
#include <string>
#include <android/log.h>
#include "include/stable-diffusion.h"

#define LOG_TAG "SdCppJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static sd_ctx_t* g_sd_ctx = nullptr;
static JavaVM* g_jvm = nullptr;
static jobject g_callback = nullptr;

static void log_callback(enum sd_log_level_t level, const char* text, void* data) {
    if (level == SD_LOG_ERROR) {
        LOGE("%s", text);
    } else {
        LOGI("%s", text);
    }
}

static void progress_callback(int step, int steps, float time, void* data) {
    if (!g_jvm || !g_callback) return;

    JNIEnv* env = nullptr;
    bool attached = false;
    int status = g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        g_jvm->AttachCurrentThread(&env, nullptr);
        attached = true;
    }
    if (!env) return;

    jclass cls = env->GetObjectClass(g_callback);
    jmethodID method = env->GetMethodID(cls, "onProgress", "(IIF)V");
    if (method) {
        env->CallVoidMethod(g_callback, method, step, steps, time);
    }

    if (attached) g_jvm->DetachCurrentThread();
}

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    return JNI_VERSION_1_6;
}

JNIEXPORT jboolean JNICALL
Java_com_gems_android_data_engine_SdCppEngine_nativeLoadModel(
    JNIEnv* env, jobject thiz,
    jstring modelPath, jstring taesdPath, jint nThreads) {

    const char* path = env->GetStringUTFChars(modelPath, nullptr);
    const char* taesd = taesdPath ? env->GetStringUTFChars(taesdPath, nullptr) : nullptr;
    LOGI("Loading model: %s, taesd: %s", path, taesd ? taesd : "none");

    sd_set_log_callback(log_callback, nullptr);
    sd_set_progress_callback(progress_callback, nullptr);

    if (g_sd_ctx) {
        free_sd_ctx(g_sd_ctx);
        g_sd_ctx = nullptr;
    }

    sd_ctx_params_t params;
    sd_ctx_params_init(&params);
    params.model_path = path;
    params.n_threads = nThreads;
    if (taesd) params.taesd_path = taesd;

    g_sd_ctx = new_sd_ctx(&params);
    env->ReleaseStringUTFChars(modelPath, path);
    if (taesd) env->ReleaseStringUTFChars(taesdPath, taesd);

    if (!g_sd_ctx) {
        LOGE("Failed to load model");
        return JNI_FALSE;
    }

    LOGI("Model loaded successfully");
    return JNI_TRUE;
}

JNIEXPORT jintArray JNICALL
Java_com_gems_android_data_engine_SdCppEngine_nativeGenerate(
    JNIEnv* env, jobject thiz,
    jstring prompt, jstring negPrompt,
    jint width, jint height, jint steps, jfloat cfgScale, jlong seed,
    jobject progressCallback) {

    if (!g_sd_ctx) {
        LOGE("Model not loaded");
        return nullptr;
    }

    // Store callback ref
    if (g_callback) env->DeleteGlobalRef(g_callback);
    g_callback = progressCallback ? env->NewGlobalRef(progressCallback) : nullptr;

    const char* promptStr = env->GetStringUTFChars(prompt, nullptr);
    const char* negStr = negPrompt ? env->GetStringUTFChars(negPrompt, nullptr) : "";
    LOGI("Generating %dx%d, steps=%d, cfg=%.1f, seed=%lld", width, height, steps, cfgScale, (long long)seed);

    sd_img_gen_params_t gen_params;
    sd_img_gen_params_init(&gen_params);
    gen_params.prompt = promptStr;
    gen_params.negative_prompt = negStr;
    gen_params.width = width;
    gen_params.height = height;
    gen_params.sample_params.sample_steps = steps;
    gen_params.sample_params.guidance.txt_cfg = cfgScale;
    gen_params.seed = seed;

    sd_image_t* result = generate_image(g_sd_ctx, &gen_params);

    env->ReleaseStringUTFChars(prompt, promptStr);
    if (negPrompt) env->ReleaseStringUTFChars(negPrompt, negStr);

    // Clean up callback
    if (g_callback) {
        env->DeleteGlobalRef(g_callback);
        g_callback = nullptr;
    }

    if (!result || !result->data) {
        LOGE("Image generation failed");
        return nullptr;
    }

    LOGI("Image generated: %dx%d, %d channels", result->width, result->height, result->channel);

    int pixelCount = result->width * result->height;
    jintArray pixels = env->NewIntArray(pixelCount);
    if (!pixels) return nullptr;

    jint* pixelData = env->GetIntArrayElements(pixels, nullptr);
    int ch = result->channel;
    for (int i = 0; i < pixelCount; i++) {
        uint8_t r = result->data[i * ch];
        uint8_t g = result->data[i * ch + 1];
        uint8_t b = result->data[i * ch + 2];
        uint8_t a = (ch == 4) ? result->data[i * ch + 3] : 255;
        pixelData[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    env->ReleaseIntArrayElements(pixels, pixelData, 0);

    free(result->data);
    free(result);

    return pixels;
}

JNIEXPORT void JNICALL
Java_com_gems_android_data_engine_SdCppEngine_nativeFree(
    JNIEnv* env, jobject thiz) {
    if (g_sd_ctx) {
        free_sd_ctx(g_sd_ctx);
        g_sd_ctx = nullptr;
        LOGI("Model freed");
    }
}

} // extern "C"
