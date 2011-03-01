#include "rhodes/JNIRhodes.h"

#include "rhodes/jni/com_rhomobile_rhodes_alert_Alert.h"

#include <common/rhoparams.h>
#include <common/RhodesApp.h>

#undef DEFAULT_LOGCATEGORY
#define DEFAULT_LOGCATEGORY "Alert"

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_alert_Alert_doCallback
  (JNIEnv *env, jclass, jstring url, jstring id, jstring title)
{
    rho_rhodesapp_callPopupCallback(rho_cast<std::string>(env, url).c_str(),
        rho_cast<std::string>(env, id).c_str(), rho_cast<std::string>(env, title).c_str());
}

RHO_GLOBAL void alert_show_status(const char* szTitle, const char* szMessage, const char* szHide)
{
    JNIEnv *env = jnienv();
    jclass cls = getJNIClass(RHODES_JAVA_CLASS_ALERT);
    if (!cls) return;
    jmethodID mid = getJNIClassStaticMethod(env, cls, "showStatusPopup", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if (!mid) return;

    RAWLOG_INFO("alert_show_status");

    env->CallStaticVoidMethod(cls, mid, rho_cast<jhstring>(szTitle).get(),
        rho_cast<jhstring>(szMessage).get(), rho_cast<jhstring>(szHide).get());
}

RHO_GLOBAL void alert_show_popup(rho_param *p)
{
    JNIEnv *env = jnienv();
    jclass cls = getJNIClass(RHODES_JAVA_CLASS_ALERT);
    if (!cls) return;
    jmethodID mid = getJNIClassStaticMethod(env, cls, "showPopup", "(Ljava/lang/Object;)V");
    if (!mid) return;

    if (p->type != RHO_PARAM_STRING && p->type != RHO_PARAM_HASH) {
        RAWLOG_ERROR("show_popup: wrong input parameter (expect String or Hash)");
        return;
    }

    jobject paramsObj = RhoValueConverter(env).createObject(p);
    env->CallStaticVoidMethod(cls, mid, paramsObj);
    env->DeleteLocalRef(paramsObj);
}

RHO_GLOBAL void alert_hide_popup()
{
    JNIEnv *env = jnienv();
    jclass cls = getJNIClass(RHODES_JAVA_CLASS_ALERT);
    if (!cls) return;
    jmethodID mid = getJNIClassStaticMethod(env, cls, "hidePopup", "()V");
    if (!mid) return;
    env->CallStaticVoidMethod(cls, mid);
}

RHO_GLOBAL void alert_vibrate(void *arg)
{
    JNIEnv *env = jnienv();
    jclass cls = getJNIClass(RHODES_JAVA_CLASS_ALERT);
    if (!cls) return;
    jmethodID mid = getJNIClassStaticMethod(env, cls, "vibrate", "(I)V");
    if (!mid) return;

    jint duration = 2500;
    if (arg)
        duration = (jint)arg;
    env->CallStaticVoidMethod(cls, mid, duration);
}

RHO_GLOBAL void alert_play_file(char* file_name, char *media_type)
{
    JNIEnv *env = jnienv();
    jclass cls = getJNIClass(RHODES_JAVA_CLASS_ALERT);
    if (!cls) return;
    jmethodID mid = getJNIClassStaticMethod(env, cls, "playFile", "(Ljava/lang/String;Ljava/lang/String;)V");
    if (!mid) return;

    env->CallStaticVoidMethod(cls, mid, rho_cast<jhstring>(file_name).get(), rho_cast<jhstring>(media_type).get());
}


RHO_GLOBAL void alert_stop()
{
    JNIEnv *env = jnienv();
    jclass cls = getJNIClass(RHODES_JAVA_CLASS_ALERT);
    if (!cls) return;
    jmethodID mid = getJNIClassStaticMethod(env, cls, "stop", "()V");
    if (!mid) return;
    env->CallStaticVoidMethod(cls, mid);
}

RHO_GLOBAL void alert_loop()
{
    JNIEnv *env = jnienv();
    jclass cls = getJNIClass(RHODES_JAVA_CLASS_ALERT);
    if (!cls) return;
    jmethodID mid = getJNIClassStaticMethod(env, cls, "loop", "()V");
    if (!mid) return;
    env->CallStaticVoidMethod(cls, mid);
}


RHO_GLOBAL void alert_pause()
{
    JNIEnv *env = jnienv();
    jclass cls = getJNIClass(RHODES_JAVA_CLASS_ALERT);
    if (!cls) return;
    jmethodID mid = getJNIClassStaticMethod(env, cls, "pause", "()V");
    if (!mid) return;
    env->CallStaticVoidMethod(cls, mid);
}

RHO_GLOBAL void alert_set_volume_percent(void *arg)
{
  JNIEnv *env = jnienv();
  jclass cls = getJNIClass(RHODES_JAVA_CLASS_ALERT);
  if (!cls) return;
  jmethodID mid = getJNIClassStaticMethod(env, cls, "setVolumePercent", "(I)V");
  if (!mid) return;
                                                                 
  jint percent = 50;
  if (arg)
    percent = (jint)arg;
  env->CallStaticVoidMethod(cls, mid, percent);
}
