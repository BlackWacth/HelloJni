#include <android/log.h>
#include <assert.h>
#include <inttypes.h>
#include <jni.h>
#include <pthread.h>
#include <string.h>

static const char *kTag = "hzw_jni";
#define LOGI(...) \
    ((void)__android_log_print(ANDROID_LOG_INFO, kTag, __VA_ARGS__))
#define LOGW(...) \
    ((void) __android_log_print(ANDROID_LOG_WARN, kTag, __VA_ARGS__))
#define LOGE(...) \
    ((void) __android_log_print(ANDROID_LOG_ERROR, kTag, __VA_ARGS__))

typedef struct tick_context {
    JavaVM *javaVm;
    jclass jniHelperClz;
    jobject jniHelperObj;
    jclass mainActivityClz;
    jobject mainActivityObj;
    pthread_mutex_t lock;
    int done;
} TickContext;
TickContext g_ctx;

void queryRuntimeInfo(JNIEnv *pInterface);

JNIEXPORT void queryRuntimeInfo(JNIEnv *env) {
    jmethodID versionFunc = (*env)->GetStaticMethodID(env, g_ctx.jniHelperClz, "getBuildVersion","()Ljava/lang/String;");
    if (!versionFunc) {
        LOGE("Failed to retrieve getBuildVersion() methodID @ line %d", __LINE__);
        return;
    }
    jstring buildVersion = (*env)->CallStaticObjectMethod(env, g_ctx.jniHelperClz, versionFunc);
    const char *version = (*env)->GetStringUTFChars(env, buildVersion, NULL);
    if (!version) {
        LOGE("Unable to get version string @ line %d", __LINE__);
        return;
    }
    LOGI("Android Version - %s", version);

    (*env)->ReleaseStringUTFChars(env, buildVersion, version);
    (*env)->DeleteLocalRef(env, buildVersion);

    jmethodID memFunc = (*env)->GetMethodID(env, g_ctx.jniHelperClz, "getRuntimeMemorySize", "()J");
    if (!memFunc) {
        LOGE("Failed to retrieve getRuntimeMemorySize() methodID @ line %d",
             __LINE__);
        return;
    }
    jlong result = (*env)->CallLongMethod(env, g_ctx.jniHelperObj, memFunc);
    LOGI("Runtime free memory size: %" PRId64, result);
}

jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    memset(&g_ctx, 0, sizeof(g_ctx));

    g_ctx.javaVm = vm;
    if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    jclass clz = (*env)->FindClass(env, "com/hzw/hellojni/JniHandler");
    g_ctx.jniHelperClz = (*env)->NewGlobalRef(env, clz);

    jmethodID jniHelperCtor = (*env)->GetMethodID(env, g_ctx.jniHelperClz, "<init>", "()V");
    jobject handler = (*env)->NewObject(env, g_ctx.jniHelperClz, jniHelperCtor);
    g_ctx.jniHelperObj = (*env)->NewGlobalRef(env, handler);
    queryRuntimeInfo(env);
    g_ctx.done = 0;
    g_ctx.mainActivityObj = NULL;
    return JNI_VERSION_1_6;
}


void sendJavaMsg(JNIEnv *env, jobject instance, jmethodID func, const char *msg) {
    jstring javaMsg = (*env)->NewStringUTF(env, msg);
    (*env)->CallVoidMethod(env, instance, func, javaMsg);
    (*env)->DeleteLocalRef(env, javaMsg);
}

void *UpdateTicks(void *context) {
    TickContext *pctx = (TickContext *) context;
    JavaVM *javaVm = pctx->javaVm;
    JNIEnv *env;
    jint res = (*javaVm)->GetEnv(javaVm, (void **) &env, JNI_VERSION_1_6);
    if (res != JNI_OK) {
        res = (*javaVm)->AttachCurrentThread(javaVm, &env, NULL);
        if (res != JNI_OK) {
            LOGE("Failed to AttachCurrentThread, ErrorCode = %d", res);
            return NULL;
        }
    }

    jmethodID statusId = (*env)->GetMethodID(env, pctx->jniHelperClz, "updateStatus","(Ljava/lang/String;)V");
    sendJavaMsg(env, pctx->jniHelperObj, statusId, "TickerThread status: initializing...");

    jmethodID timerId = (*env)->GetMethodID(env, pctx->mainActivityClz, "updateTimer", "()V");
    struct timeval beginTime, curTime, usedTime, leftTime;
    const struct timeval kOneSecond = {(__kernel_time_t) 1, (__kernel_suseconds_t) 0};
    sendJavaMsg(env, pctx->jniHelperObj, statusId, "TickerThread status: start ticking ...");

    while (1) {
        gettimeofday(&beginTime, NULL);
        pthread_mutex_lock(&pctx->lock);
        int done = pctx->done;
        if (pctx->done) {
            pctx->done = 0;
        }
        pthread_mutex_unlock(&pctx->lock);
        if (done) {
            break;
        }
        (*env)->CallVoidMethod(env, pctx->mainActivityObj, timerId);

        gettimeofday(&curTime, NULL);
        timersub(&curTime, &beginTime, &usedTime);
        timersub(&kOneSecond, &usedTime, &leftTime);
        struct timespec sleepTime;
        sleepTime.tv_sec = leftTime.tv_sec;
        sleepTime.tv_nsec = leftTime.tv_usec * 1000;

        if (sleepTime.tv_sec <= 1) {
            nanosleep(&sleepTime, NULL);
        } else {
            sendJavaMsg(env, pctx->jniHelperObj, statusId,"TickerThread error: processing too long!");
        }
    }
    sendJavaMsg(env, pctx->jniHelperObj, statusId, "TickerThread status: ticking stopped");
    (*javaVm)->DetachCurrentThread(javaVm);
    return context;
}


JNIEXPORT void JNICALL
Java_com_hzw_hellojni_MainActivity_startTicks(JNIEnv *env, jobject instance) {
    pthread_t threadInfo;
    pthread_attr_t threadAttr;

    pthread_attr_init(&threadAttr);
    pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED);

    pthread_mutex_init(&g_ctx.lock, NULL);

    jclass clz = (*env)->GetObjectClass(env, instance);
    g_ctx.mainActivityClz = (*env)->NewGlobalRef(env, clz);
    g_ctx.mainActivityObj = (*env)->NewGlobalRef(env, instance);

    int result = pthread_create(&threadInfo, &threadAttr, UpdateTicks, &g_ctx);
    assert(result == 0);
    pthread_attr_destroy(&threadAttr);
}

JNIEXPORT void JNICALL
Java_com_hzw_hellojni_MainActivity_stopTicks(JNIEnv *env, jobject instance) {
    pthread_mutex_lock(&g_ctx.lock);
    g_ctx.done = 1;
    pthread_mutex_unlock(&g_ctx.lock);

    struct timespec sleepTime;
    memset(&sleepTime, 0, sizeof(sleepTime));
    sleepTime.tv_nsec = 100000000;
    while (g_ctx.done) {
        nanosleep(&sleepTime, NULL);
    }

    (*env)->DeleteGlobalRef(env, g_ctx.mainActivityClz);
    (*env)->DeleteGlobalRef(env, g_ctx.mainActivityObj);
    g_ctx.mainActivityClz = NULL;
    g_ctx.mainActivityObj = NULL;

    pthread_mutex_destroy(&g_ctx.lock);
}