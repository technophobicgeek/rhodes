#include <android/log.h>

#include "rhodes/jni/com_rhomobile_rhodes_RhodesService.h"
#include "rhodes/jni/com_rhomobile_rhodes_RhodesAppOptions.h"

#include <common/RhoConf.h>
#include <common/app_build_configs.h>
#include <logging/RhoLogConf.h>
#include <common/RhodesApp.h>
#include <sync/SyncThread.h>
#include <sync/ClientRegister.h>

#include <sys/stat.h>
#include <sys/resource.h>

#include "rhodes/JNIRhodes.h"
#include "rhodes/RhoClassFactory.h"

#undef DEFAULT_LOGCATEGORY
#define DEFAULT_LOGCATEGORY "Rhodes"

const char *rho_java_class[] = {
#define RHODES_DEFINE_JAVA_CLASS(x, name) name,
#include <rhodes/details/rhojava.inc>
#undef RHODES_DEFINE_JAVA_CLASS
};

static std::string g_root_path;
static std::string g_sqlite_journals_path;
static std::string g_apk_path;

static pthread_key_t g_thrkey;

static JavaVM *g_jvm = NULL;
JavaVM *jvm()
{
    return g_jvm;
}

namespace rho
{
namespace common
{

class AndroidLogSink : public ILogSink
{
public:
    void writeLogMessage(String &strMsg)
    {
        __android_log_write(ANDROID_LOG_INFO, "APP", strMsg.c_str());
    }

    int getCurPos()
    {
        return 0;
    }

    void clear() {}
};

static CAutoPtr<AndroidLogSink> g_androidLogSink(new AndroidLogSink());

} // namespace common
} // namespace rho

RhoValueConverter::RhoValueConverter(JNIEnv *e)
    :env(e), init(false)
{
    clsHashMap = getJNIClass(RHODES_JAVA_CLASS_HASHMAP);
    if (!clsHashMap) return;
    clsVector = getJNIClass(RHODES_JAVA_CLASS_VECTOR);
    if (!clsVector) return;
    midHashMapConstructor = getJNIClassMethod(env, clsHashMap, "<init>", "()V");
    if (!midHashMapConstructor) return;
    midVectorConstructor = getJNIClassMethod(env, clsVector, "<init>", "()V");
    if (!midVectorConstructor) return;
    midPut = getJNIClassMethod(env, clsHashMap, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    if (!midPut) return;
    midAddElement = getJNIClassMethod(env, clsVector, "addElement", "(Ljava/lang/Object;)V");
    if (!midAddElement) return;
    init = true;
}

jobject RhoValueConverter::createObject(rho_param *p)
{
    if (!init || !p) return NULL;
    switch (p->type) {
    case RHO_PARAM_STRING:
        return rho_cast<jhstring>(p->v.string).release();
        break;
    case RHO_PARAM_ARRAY:
        {
            jobject v = env->NewObject(clsVector, midVectorConstructor);
            if (!v) return NULL;

            for (int i = 0, lim = p->v.array->size; i < lim; ++i) {
                jhobject value = jhobject(createObject(p->v.array->value[i]));
                env->CallVoidMethod(v, midAddElement, value.get());
            }
            return v;
        }
        break;
    case RHO_PARAM_HASH:
        {
            jobject v = env->NewObject(clsHashMap, midHashMapConstructor);
            if (!v) return NULL;

            for (int i = 0, lim = p->v.hash->size; i < lim; ++i) {
                jhstring key = rho_cast<jhstring>(p->v.hash->name[i]);
                jhobject value = jhobject(createObject(p->v.hash->value[i]));
                env->CallObjectMethod(v, midPut, key.get(), value.get());
            }
            return v;
        }
        break;
    default:
        return NULL;
    }
}

void store_thr_jnienv(JNIEnv *env)
{
    pthread_setspecific(g_thrkey, env);
}

JNIEnv *jnienv()
{
    JNIEnv *env = (JNIEnv *)pthread_getspecific(g_thrkey);
    if (!env)
        RAWLOG_ERROR("JNIEnv is not set for this thread!!!");
    return env;
}

std::vector<jclass> g_classes;

jclass getJNIClass(int n)
{
    if (n < 0 || (size_t)n >= g_classes.size())
    {
        RAWLOG_ERROR1("Illegal index when call getJNIClass: %d", n);
        return NULL;
    }
    return g_classes[n];
}

jclass getJNIObjectClass(JNIEnv *env, jobject obj)
{
    jclass cls = env->GetObjectClass(obj);
    if (!cls)
        RAWLOG_ERROR1("Can not get class for object: %p (JNI)", obj);
    return cls;
}

jfieldID getJNIClassField(JNIEnv *env, jclass cls, const char *name, const char *signature)
{
    jfieldID fid = env->GetFieldID(cls, name, signature);
    if (!fid)
        RAWLOG_ERROR3("Can not get field %s of signature %s for class %p", name, signature, cls);
    return fid;
}

jfieldID getJNIClassStaticField(JNIEnv *env, jclass cls, const char *name, const char *signature)
{
    jfieldID fid = env->GetStaticFieldID(cls, name, signature);
    if (!fid)
        RAWLOG_ERROR3("Can not get static field %s of signature %s for class %p", name, signature, cls);
    return fid;
}

jmethodID getJNIClassMethod(JNIEnv *env, jclass cls, const char *name, const char *signature)
{
    jmethodID mid = env->GetMethodID(cls, name, signature);
    if (!mid)
        RAWLOG_ERROR3("Can not get method %s of signature %s for class %p", name, signature, cls);
    return mid;
}

jmethodID getJNIClassStaticMethod(JNIEnv *env, jclass cls, const char *name, const char *signature)
{
    jmethodID mid = env->GetStaticMethodID(cls, name, signature);
    if (!mid)
        RAWLOG_ERROR3("Can not get static method %s of signature %s for class %p", name, signature, cls);
    return mid;
}

std::string const &rho_root_path()
{
    return g_root_path;
}

const char* rho_native_rhopath()
{
    return rho_root_path().c_str();
}

std::string const &rho_apk_path()
{
    return g_apk_path;
}

jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/)
{
    g_jvm = vm;
    jint jversion = JNI_VERSION_1_4;
    JNIEnv *env;
    if (vm->GetEnv((void**)&env, jversion) != JNI_OK)
        return -1;

    pthread_key_create(&g_thrkey, NULL);
    store_thr_jnienv(env);

    for(size_t i = 0, lim = sizeof(rho_java_class)/sizeof(rho_java_class[0]); i != lim; ++i)
    {
        const char *className = rho_java_class[i];
        jclass cls = env->FindClass(className);
        if (!cls)
            return -1;
        g_classes.push_back((jclass)env->NewGlobalRef(cls));
        env->DeleteLocalRef(cls);
    }

    return jversion;
}

namespace details
{

std::string rho_cast_helper<std::string, jstring>::operator()(JNIEnv *env, jstring s)
{
    const char *ts = env->GetStringUTFChars(s, JNI_FALSE);
    std::string ret(ts);
    env->ReleaseStringUTFChars(s, ts);
    return ret;
}

jhstring rho_cast_helper<jhstring, char const *>::operator()(JNIEnv *env, char const *s)
{
    return jhstring(env->NewStringUTF(s));
}

static rho::common::CMutex rho_cast_java_ruby_mtx;

static jclass clsString;
static jclass clsMap;
static jclass clsSet;
static jclass clsIterator;

static jmethodID midMapGet;
static jmethodID midMapKeySet;
static jmethodID midSetIterator;
static jmethodID midIteratorHasNext;
static jmethodID midIteratorNext;

static bool rho_cast_java_ruby_init(JNIEnv *env)
{
    static rho::common::CMutex rho_fd_mtx;
    static bool initialized = false;
    if (initialized)
        return true;

    rho::common::CMutexLock guard(rho_cast_java_ruby_mtx);
    if (initialized)
        return true;

    clsString = getJNIClass(RHODES_JAVA_CLASS_STRING);
    if (!clsString) return false;
    clsMap = getJNIClass(RHODES_JAVA_CLASS_MAP);
    if (!clsMap) return false;
    clsSet = getJNIClass(RHODES_JAVA_CLASS_SET);
    if (!clsSet) return false;
    clsIterator = getJNIClass(RHODES_JAVA_CLASS_ITERATOR);
    if (!clsIterator) return false;

    midMapGet = getJNIClassMethod(env, clsMap, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
    if (!midMapGet) return false;
    midMapKeySet = getJNIClassMethod(env, clsMap, "keySet", "()Ljava/util/Set;");
    if (!midMapKeySet) return false;
    midSetIterator = getJNIClassMethod(env, clsSet, "iterator", "()Ljava/util/Iterator;");
    if (!midSetIterator) return false;
    midIteratorHasNext = getJNIClassMethod(env, clsIterator, "hasNext", "()Z");
    if (!midIteratorHasNext) return false;
    midIteratorNext = getJNIClassMethod(env, clsIterator, "next", "()Ljava/lang/Object;");
    if (!midIteratorNext) return false;

    initialized = true;
    return true;
}

static VALUE convertJavaMapToRubyHash(JNIEnv *env, jobject objMap)
{
    jobject objSet = env->CallObjectMethod(objMap, midMapKeySet);
    if (!objSet) return Qnil;
    jobject objIterator = env->CallObjectMethod(objSet, midSetIterator);
    if (!objIterator) return Qnil;
                                  
    CHoldRubyValue retval(rho_ruby_createHash());
    while(env->CallBooleanMethod(objIterator, midIteratorHasNext))
    {
        jstring objKey = (jstring)env->CallObjectMethod(objIterator, midIteratorNext);
        if (!objKey) return Qnil;
        jstring objValue = (jstring)env->CallObjectMethod(objMap, midMapGet, objKey);
        if (!objValue) return Qnil;

        std::string const &strKey = rho_cast<std::string>(objKey);
        std::string const &strValue = rho_cast<std::string>(objValue);
        addStrToHash(retval, strKey.c_str(), strValue.c_str());

        env->DeleteLocalRef(objKey);
        env->DeleteLocalRef(objValue);
    }
    return retval;
}

jobject rho_cast_helper<jobject, VALUE>::operator()(JNIEnv *env, VALUE value)
{
    if (!rho_cast_java_ruby_init(env))
    {
        env->ThrowNew(getJNIClass(RHODES_JAVA_CLASS_RUNTIME_EXCEPTION), "Java <=> Ruby conversion initialization failed");
        return NULL;
    }

    if (NIL_P(value))
        return NULL;

    if (TYPE(value) == T_STRING)
        return env->NewStringUTF(RSTRING_PTR(value));

    RAWLOG_ERROR("rho_cast<jobject, VALUE>: unknown type of value");
    return NULL;
}

VALUE rho_cast_helper<VALUE, jobject>::operator()(JNIEnv *env, jobject obj)
{
    if (!rho_cast_java_ruby_init(env))
    {
        env->ThrowNew(getJNIClass(RHODES_JAVA_CLASS_RUNTIME_EXCEPTION), "Java <=> Ruby conversion initialization failed");
        return Qnil;
    }

    if (!obj)
        return Qnil;

    if (env->IsInstanceOf(obj, clsString))
        return rho_ruby_create_string(rho_cast<std::string>(env, (jstring)obj).c_str());

    if (env->IsInstanceOf(obj, clsMap))
        return convertJavaMapToRubyHash(env, obj);

    RAWLOG_ERROR("rho_cast<VALUE, jobject>: unknown type of value");
    return Qnil;
}

} // namespace details

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_RhodesService_makeLink
  (JNIEnv *env, jclass, jstring src, jstring dst)
{
    // We should not use rho_cast functions here because this function
    // called very early when jnienv() is not yet initialized
    const char *strSrc = env->GetStringUTFChars(src, JNI_FALSE);
    const char *strDst = env->GetStringUTFChars(dst, JNI_FALSE);
    ::unlink(strDst);
    int err = ::symlink(strSrc, strDst);
    env->ReleaseStringUTFChars(src, strSrc);
    env->ReleaseStringUTFChars(dst, strDst);
    if (err != 0)
        env->ThrowNew(getJNIClass(RHODES_JAVA_CLASS_RUNTIME_EXCEPTION), "Can not create symlink");
}

static bool set_capabilities(JNIEnv *env)
{
    char const *caps[] = {
#define RHO_DEFINE_CAP(x) #x,
#include <rhocaps.inc>
#undef RHO_DEFINE_CAP
    };
    std::map<std::string, bool> actual_caps;
#define RHO_DEFINE_CAP(x) actual_caps[#x] = RHO_CAP_ ## x ## _ENABLED;
#include <rhocaps.inc>
#undef RHO_DEFINE_CAP

    jclass cls = getJNIClass(RHODES_JAVA_CLASS_CAPABILITIES);
    if (!cls) return false;
    for (size_t i = 0, lim = sizeof(caps)/sizeof(caps[0]); i < lim; ++i)
    {
        std::string field_name = std::string(caps[i]) + "_ENABLED";
        jfieldID fid = getJNIClassStaticField(env, cls, field_name.c_str(), "Z");
        if (!fid) return false;
        env->SetStaticBooleanField(cls, fid, actual_caps[caps[i]]);
    }
    return true;
}

static jobject g_classLoader = NULL;
static jmethodID g_loadClass = NULL;

jclass rho_find_class(JNIEnv *env, const char *c)
{
    jstring className = env->NewStringUTF(c);
    jclass cls = (jclass)env->CallObjectMethod(g_classLoader, g_loadClass, className);
    env->DeleteLocalRef(className);
    return cls;
}

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_RhodesService_initClassLoader
  (JNIEnv *env, jobject, jobject cl)
{
    g_classLoader = env->NewGlobalRef(cl);
    jclass javaLangClassLoader = env->FindClass("java/lang/ClassLoader");
    g_loadClass = env->GetMethodID(javaLangClassLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
}

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_RhodesService_nativeInitPath
  (JNIEnv *env, jobject, jstring root_path, jstring sqlite_journals_path, jstring apk_path)
{
    g_root_path = rho_cast<std::string>(env, root_path);
    g_sqlite_journals_path = rho_cast<std::string>(env, sqlite_journals_path);
    g_apk_path = rho_cast<std::string>(env, apk_path);
}

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_RhodesService_createRhodesApp
  (JNIEnv *env, jobject)
{
    jclass clsRE = getJNIClass(RHODES_JAVA_CLASS_RUNTIME_EXCEPTION);
    if (!clsRE)
        return;

    struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim) == -1)
    {
        env->ThrowNew(clsRE, "Can not get maximum number of open files");
        return;
    }
    if (rlim.rlim_max < (unsigned long)RHO_FD_BASE)
    {
        env->ThrowNew(clsRE, "Current limit of open files is less then RHO_FD_BASE");
        return;
    }
    if (rlim.rlim_cur > (unsigned long)RHO_FD_BASE)
    {
        rlim.rlim_cur = RHO_FD_BASE;
        rlim.rlim_max = RHO_FD_BASE;
        if (setrlimit(RLIMIT_NOFILE, &rlim) == -1)
        {
            env->ThrowNew(clsRE, "Can not set maximum number of open files");
            return;
        }
    }

    if (!set_capabilities(env)) return;

    // Init SQLite temp directory
    sqlite3_temp_directory = (char*)g_sqlite_journals_path.c_str();

    const char* szRootPath = rho_native_rhopath();

    // Init logconf
    rho_logconf_Init(szRootPath);

    // Disable log to stdout as on android all stdout redirects to /dev/null
    RHOCONF().setBool("LogToOutput", false, true);
    LOGCONF().setLogToOutput(false);
    // Add android system log sink
    LOGCONF().setLogView(rho::common::g_androidLogSink);

    // Start Rhodes application
    rho_rhodesapp_create(szRootPath);
}

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_RhodesService_startRhodesApp
  (JNIEnv *env, jobject obj)
{
    rho_rhodesapp_start();
}

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_RhodesService_stopRhodesApp
  (JNIEnv *, jobject)
{
    rho_rhodesapp_destroy();
}

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_RhodesService_doSyncAllSources
  (JNIEnv *, jobject, jboolean show_status_popup)
{
    rho_sync_doSyncAllSources(show_status_popup);
}

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_RhodesService_doSyncSource
  (JNIEnv *env, jobject, jstring sourceObj)
{
    std::string source = rho_cast<std::string>(env, sourceObj);
    rho_sync_doSyncSourceByName(source.c_str());
}

RHO_GLOBAL jstring JNICALL Java_com_rhomobile_rhodes_RhodesAppOptions_getOptionsUrl
  (JNIEnv *env, jclass)
{
    const char *s = RHODESAPP().getOptionsUrl().c_str();
    return rho_cast<jhstring>(env, s).release();
}

RHO_GLOBAL jstring JNICALL Java_com_rhomobile_rhodes_RhodesAppOptions_getStartUrl
  (JNIEnv *env, jclass)
{
    const char *s = RHODESAPP().getStartUrl().c_str();
    return rho_cast<jhstring>(env, s).release();
}

RHO_GLOBAL jstring JNICALL Java_com_rhomobile_rhodes_RhodesAppOptions_getCurrentUrl
  (JNIEnv *env, jclass)
{
    const char *s = RHODESAPP().getCurrentUrl(0).c_str();
    return rho_cast<jhstring>(env, s).release();
}

RHO_GLOBAL jstring JNICALL Java_com_rhomobile_rhodes_RhodesAppOptions_getAppBackUrl
  (JNIEnv *env, jclass)
{
    const char *s = RHODESAPP().getAppBackUrl().c_str();
    return rho_cast<jhstring>(env, s).release();
}

RHO_GLOBAL jstring JNICALL Java_com_rhomobile_rhodes_RhodesAppOptions_getBlobPath
  (JNIEnv *env, jclass)
{
    const char *s = RHODESAPP().getBlobsDirPath().c_str();
    return rho_cast<jhstring>(env, s).release();
}

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_RhodesService_doRequest
  (JNIEnv *env, jobject, jstring strUrl)
{
    std::string const &url = rho_cast<std::string>(strUrl);
    rho_net_request(url.c_str());
}

RHO_GLOBAL jstring JNICALL Java_com_rhomobile_rhodes_RhodesService_normalizeUrl
  (JNIEnv *env, jobject, jstring strUrl)
{
    std::string const &s = rho_cast<std::string>(strUrl);
    std::string const &cs = RHODESAPP().canonicalizeRhoUrl(s);
    return rho_cast<jhstring>(env, cs).release();
}

RHO_GLOBAL jstring JNICALL Java_com_rhomobile_rhodes_RhodesService_getBuildConfig
  (JNIEnv *env, jclass, jstring key)
{
    std::string const &s = rho_cast<std::string>(key);
    const char* cs = get_app_build_config_item(s.c_str());
    return rho_cast<jhstring>(env, cs).release();
}

RHO_GLOBAL jboolean JNICALL Java_com_rhomobile_rhodes_RhodesService_isOnStartPage
  (JNIEnv *env, jclass)
{
    bool res = RHODESAPP().isOnStartPage();
    return (jboolean)res;
}

JNIEXPORT jboolean JNICALL Java_com_rhomobile_rhodes_RhodesService_canStartApp
  (JNIEnv *, jclass, jstring cmdLine, jstring sep)
{
    std::string const &strCmdLine = rho_cast<std::string>(cmdLine);
    std::string const &strSep = rho_cast<std::string>(sep);

    int nRes = rho_rhodesapp_canstartapp(strCmdLine.c_str(), strSep.c_str());
    return (jboolean)(nRes ? true : false);
}

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_RhodesService_navigateBack
  (JNIEnv *, jclass)
{
    rho_rhodesapp_navigate_back();
}

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_RhodesService_loadUrl
  (JNIEnv *env, jclass, jstring str)
{
    rho_rhodesapp_load_url(rho_cast<std::string>(env, str).c_str());
}

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_RhodesService_onScreenOrientationChanged
  (JNIEnv *env, jclass, jint width, jint height, jint angle)
{
	rho_rhodesapp_callScreenRotationCallback(width, height, angle);
	//RAWLOG_ERROR3("$$$$$$$$$$$$$$$$ SCREEN : [%d]x[%d] angle[%d]", width, height, angle);	
}

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_RhodesService_callUiCreatedCallback
  (JNIEnv *, jobject)
{
    rho_rhodesapp_callUiCreatedCallback();
}

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_RhodesService_callUiDestroyedCallback
  (JNIEnv *, jobject)
{
    rho_rhodesapp_callUiDestroyedCallback();
}

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_RhodesService_callActivationCallback
  (JNIEnv *, jobject, jboolean active)
{
    rho_rhodesapp_callAppActiveCallback(active);
}

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_RhodesService_setPushRegistrationId
  (JNIEnv *env, jobject, jstring jId)
{
    std::string id = rho_cast<std::string>(env, jId);
    rho::sync::CClientRegister::Create(id.c_str());
}

RHO_GLOBAL jboolean JNICALL Java_com_rhomobile_rhodes_RhodesService_callPushCallback
  (JNIEnv *env, jobject, jstring jData)
{
    std::string data = rho_cast<std::string>(env, jData);
    return (jboolean)rho_rhodesapp_callPushCallback(data.c_str());
}

RHO_GLOBAL char *rho_timezone()
{
    static char *tz = NULL;
    if (!tz)
    {
        JNIEnv *env = jnienv();
        jclass cls = getJNIClass(RHODES_JAVA_CLASS_RHODES_SERVICE);
        if (!cls) return NULL;
        jmethodID mid = getJNIClassStaticMethod(env, cls, "getTimezoneStr", "()Ljava/lang/String;");
        if (!mid) return NULL;
        jstring s = (jstring)env->CallStaticObjectMethod(cls, mid);
        std::string tzs = rho_cast<std::string>(env, s);
        tz = strdup(tzs.c_str());
    }
    return tz;
}
