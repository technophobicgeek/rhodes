#include "rhodes/JNIRhodes.h"

#include <dlfcn.h>
#include <android/log.h>

#include <hash_map>

#include "rhodes/jni/com_rhomobile_rhodes_file_RhoFileApi.h"

#ifdef RHO_LOG
#undef RHO_LOG
#endif
#define RHO_LOG(...)

#ifdef RHO_NOT_IMPLEMENTED
#undef RHO_NOT_IMPLEMENTED
#endif
#define RHO_NOT_IMPLEMENTED \
  __android_log_print(ANDROID_LOG_ERROR, "RhoFileApiNotImpl", \
    "WARNING: Call not implemented function: \"%s\" (defined here: %s:%d)", __PRETTY_FUNCTION__, __FILE__, __LINE__); \
  errno = EACCES; \
  return -1

enum rho_fileapi_type_t
{
    rho_type_file,
    rho_type_dir
};

struct rho_stat_t
{
    rho_fileapi_type_t type;
    size_t size;
    unsigned long mtime;
};

typedef std::hash_map<std::string, rho_stat_t> rho_stat_map_t;
static rho_stat_map_t rho_stat_map;

struct rho_fd_data_t
{
    jobject is;
    std::string fpath;
    loff_t pos;
};

typedef rho::common::CMutexLock scoped_lock_t;
static rho::common::CMutex rho_fd_mtx;
typedef std::map<int, rho_fd_data_t> rho_fd_map_t;
rho_fd_map_t rho_fd_map;
static std::vector<int> rho_fd_free;
static int rho_fd_counter = RHO_FD_BASE;

static jclass clsFileApi;
static jmethodID midCopy;
static jmethodID midOpen;
static jmethodID midClose;
static jmethodID midRead;
static jmethodID midSeek;

typedef FILE *(*func_sfp_t)();
typedef int (*func_sflags_t)(const char *mode, int *optr);

static func_sflags_t __sflags;
static func_sfp_t __sfp;

typedef int (*func_access_t)(const char *path, int mode);
typedef int (*func_close_t)(int fd);
typedef int (*func_dup2_t)(int fd, int fd2);
typedef int (*func_dup_t)(int fd);
typedef int (*func_fchdir_t)(int fd);
typedef int (*func_fcntl_t)(int fd, int command, ...);
typedef int (*func_fdatasync_t)(int fd);
typedef int (*func_flock_t)(int fd, int operation);
typedef int (*func_fstat_t)(int filedes, struct stat *buf);
typedef int (*func_fsync_t)(int fd);
typedef int (*func_ftruncate_t)(int fd, off_t offset);
typedef int (*func_lstat_t)(const char *path, struct stat *buf);
typedef int (*func_open_t)(const char *path, int oflag, ...);
typedef int (*func_select_t)(int maxfd, fd_set *rfd, fd_set *wfd, fd_set *efd, struct timeval *tv);
typedef int (*func_stat_t)(const char *path, struct stat *buf);
typedef int (*func_unlink_t)(const char *path);
typedef loff_t (*func_lseek64_t)(int fd, loff_t offset, int whence);
typedef off_t (*func_lseek_t)(int fd, off_t offset, int whence);
typedef ssize_t (*func_read_t)(int fd, void *buf, size_t count);
typedef ssize_t (*func_write_t)(int fd, const void *buf, size_t count);

static func_access_t real_access;
static func_close_t real_close;
static func_dup2_t real_dup2;
static func_dup_t real_dup;
static func_fchdir_t real_fchdir;
static func_fcntl_t real_fcntl;
static func_fdatasync_t real_fdatasync;
static func_flock_t real_flock;
static func_fstat_t real_fstat;
static func_fsync_t real_fsync;
static func_ftruncate_t real_ftruncate;
static func_lseek64_t real_lseek64;
static func_lseek_t real_lseek;
static func_lstat_t real_lstat;
static func_open_t real_open;
static func_read_t real_read;
static func_select_t real_select;
static func_stat_t real_stat;
static func_unlink_t real_unlink;
static func_write_t real_write;

struct stat librhodes_st;

static bool has_pending_exception()
{
    JNIEnv *env = jnienv();
    if (env->ExceptionCheck())
    {
        __android_log_print(ANDROID_LOG_ERROR, "fileapi", "ERROR!!! Pending exception exist!");
        return true;
    }
    return false;
}

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_file_RhoFileApi_updateStatTable
  (JNIEnv *env, jclass, jstring pathObj, jstring type, jlong size, jlong mtime)
{
    std::string path = rho_cast<std::string>(env, pathObj);
    rho_stat_t st;
    std::string t = rho_cast<std::string>(env, type);
    if (t == "dir")
        st.type = rho_type_dir;
    else if (t == "file")
        st.type = rho_type_file;
    else
    {
        jclass clsRE = getJNIClass(RHODES_JAVA_CLASS_RUNTIME_EXCEPTION);
        if (!clsRE) return;
        env->ThrowNew(clsRE, "Unknown type of file");
        return;
    }
    st.size = (size_t)size;
    st.mtime = (unsigned long)mtime;

    rho_stat_map.insert(std::make_pair(path, st));

    if (st.type == rho_type_dir)
    {
        std::string fpath = rho_root_path() + "/" + path;
        RHO_LOG("updateStatTable: create dir: %s", fpath.c_str());
        mkdir(fpath.c_str(), S_IRWXU);
    }
}

RHO_GLOBAL void JNICALL Java_com_rhomobile_rhodes_file_RhoFileApi_nativeInit
  (JNIEnv *env, jclass)
{
    clsFileApi = getJNIClass(RHODES_JAVA_CLASS_RHOFILEAPI);
    if (!clsFileApi) return;
    midCopy = getJNIClassStaticMethod(env, clsFileApi, "copy", "(Ljava/lang/String;)Z");
    if (!midCopy) return;
    midOpen = getJNIClassStaticMethod(env, clsFileApi, "openInPackage", "(Ljava/lang/String;)Ljava/io/InputStream;");
    if (!midOpen) return;
    midClose = getJNIClassStaticMethod(env, clsFileApi, "close", "(Ljava/io/InputStream;)V");
    if (!midClose) return;
    midRead = getJNIClassStaticMethod(env, clsFileApi, "read", "(Ljava/io/InputStream;[B)I");
    if (!midRead) return;
    midSeek = getJNIClassStaticMethod(env, clsFileApi, "seek", "(Ljava/io/InputStream;I)V");
    if (!midSeek) return;

    const char *libc = "/system/lib/libc.so";
    void *pc = dlopen(libc, RTLD_LAZY);
    __sflags = (func_sflags_t)dlsym(pc, "__sflags");
    __sfp = (func_sfp_t)dlsym(pc, "__sfp");
    real_access = (func_access_t)dlsym(pc, "access");
    real_close = (func_close_t)dlsym(pc, "close");
    real_dup = (func_dup_t)dlsym(pc, "dup");
    real_dup2 = (func_dup2_t)dlsym(pc, "dup2");
    real_fchdir = (func_fchdir_t)dlsym(pc, "fchdir");
    real_fcntl = (func_fcntl_t)dlsym(pc, "fcntl");
    real_fdatasync = (func_fdatasync_t)dlsym(pc, "fdatasync");
    real_flock = (func_flock_t)dlsym(pc, "flock");
    real_fstat = (func_fstat_t)dlsym(pc, "fstat");
    real_fsync = (func_fsync_t)dlsym(pc, "fsync");
    real_ftruncate = (func_ftruncate_t)dlsym(pc, "ftruncate");
    real_lseek = (func_lseek_t)dlsym(pc, "lseek");
    real_lseek64 = (func_lseek64_t)dlsym(pc, "lseek64");
    real_lstat = (func_lstat_t)dlsym(pc, "lstat");
    real_open = (func_open_t)dlsym(pc, "open");
    real_read = (func_read_t)dlsym(pc, "read");
    real_select = (func_select_t)dlsym(pc, "select");
    real_stat = (func_stat_t)dlsym(pc, "stat");
    real_unlink = (func_unlink_t)dlsym(pc, "unlink");
    real_write = (func_write_t)dlsym(pc, "write");
    dlclose(pc);

    // This is just to get typical stat of file
    std::string librhodes = rho_root_path() + "../lib/librhodes.so";
    real_stat(librhodes.c_str(), &librhodes_st);
    librhodes_st.st_mode = S_IFREG|S_IRWXU;
    librhodes_st.st_nlink = 1;
    librhodes_st.st_uid = getuid();
    librhodes_st.st_gid = getgid();
}

static std::string normalize_path(std::string path)
{
    //RHO_LOG("normalize_path: (1): path: %s", path.c_str());

    bool has_leading_slash = !path.empty() && path[0] == '/';

    std::vector<std::string> parts;
    for (;;)
    {
        size_t idx = path.find('/');
        std::string part = path.substr(0, idx);
        if (part == "..")
        {
            //RHO_LOG("normalize_path: (2)");
            if (!parts.empty())
                parts.erase(parts.end() - 1);
        }
        else if (!part.empty() && part != ".")
        {
            //RHO_LOG("normalize_path: (3): part: %s", part.c_str());
            parts.push_back(part);
        }
        if (idx == std::string::npos)
            break;
        path = path.substr(idx + 1);
        //RHO_LOG("normalize_path: (4): path: %s", path.c_str());
    }

    path.clear();
    for (size_t i = 0, lim = parts.size(); i < lim; ++i)
    {
        path += "/";
        path += parts[i];
    }

    if (!has_leading_slash)
        path.erase(0, 1);

    //RHO_LOG("normalize_path: return %s", path.c_str());
    return path;
}

RHO_GLOBAL jstring JNICALL Java_com_rhomobile_rhodes_file_RhoFileApi_normalizePath
  (JNIEnv *env, jclass, jstring pathObj)
{
    std::string path = normalize_path(rho_cast<std::string>(env, pathObj));
    return rho_cast<jhstring>(env, path).release();
}

static std::string make_full_path(const char *path)
{
    //RHO_LOG("make_full_path: %s", path);
    if (path == NULL || *path == '\0')
        return "";

    if (*path == '/')
        return path;

    std::string fpath = rho_root_path() + "/" + path;

    return normalize_path(fpath);
}

static std::string make_full_path(std::string const &path)
{
    return make_full_path(path.c_str());
}

static std::string make_rel_path(std::string const &fpath)
{
    std::string const &root_path = rho_root_path();
    return fpath.substr(root_path.size());
}

RHO_GLOBAL jstring JNICALL Java_com_rhomobile_rhodes_file_RhoFileApi_makeRelativePath
  (JNIEnv *env, jclass, jstring pathObj)
{
    std::string path = rho_cast<std::string>(env, pathObj);
    return rho_cast<jhstring>(env, make_rel_path(make_full_path(path))).release();
}

static rho_stat_t *rho_stat(const char *path)
{
    std::string relpath = make_rel_path(make_full_path(path));
    rho_stat_map_t::iterator it = rho_stat_map.find(relpath);
    if (it == rho_stat_map.end())
        return NULL;
    return &(it->second);
}

static rho_stat_t *rho_stat(std::string const &path)
{
    return rho_stat(path.c_str());
}

#if 0
static rho_stat_t *rho_stat(int fd)
{
    if (fd < RHO_FD_BASE)
        return NULL;

    scoped_lock_t guard(rho_fd_mtx);

    rho_fd_map_t::iterator it = rho_fd_map.find(fd);
    if (it == rho_fd_map.end())
        return NULL;

    return rho_stat(make_rel_path(it->second.fpath));
}
#endif

#if 0
static void dump_stat(struct stat const &st)
{
    RHO_LOG("st.st_dev: %llx", (long long)st.st_dev);
    RHO_LOG("st.st_ino: %lx", (long)st.st_ino);
    RHO_LOG("st.st_mode: %x", (int)st.st_mode);
    RHO_LOG("st.st_nlink: %x", (int)st.st_nlink);
    RHO_LOG("st.st_uid: %lx", (unsigned long)st.st_uid);
    RHO_LOG("st.st_gid: %lx", (unsigned long)st.st_gid);
    RHO_LOG("st.st_rdev: %llx", (long long)st.st_rdev);
    RHO_LOG("st.st_size: %lld", (long long)st.st_size);
    RHO_LOG("st.st_blksize: %lu", (unsigned long)st.st_blksize);
    RHO_LOG("st.st_blocks: %llu", (unsigned long long)st.st_blocks);
    RHO_LOG("st.st_atime: %lu", (unsigned long)st.st_atime);
    RHO_LOG("st.st_atime_nsec: %lu", (unsigned long)st.st_atime_nsec);
    RHO_LOG("st.st_mtime: %lu", (unsigned long)st.st_mtime);
    RHO_LOG("st.st_mtime_nsec: %lu", (unsigned long)st.st_mtime_nsec);
    RHO_LOG("st.st_ctime: %lu", (unsigned long)st.st_ctime);
    RHO_LOG("st.st_ctime_nsec: %lu", (unsigned long)st.st_ctime_nsec);
    RHO_LOG("st.st_ino: %llx", (unsigned long long)st.st_ino);
}
#endif

static bool need_java_way(std::string const &path)
{
    //RHO_LOG("need_java_way: %s", path.c_str());
    std::string fpath = make_full_path(path);
    //RHO_LOG("need_java_way: (1): %s", fpath.c_str());
    std::string const &root_path = rho_root_path();
    if (strncmp(fpath.c_str(), root_path.c_str(), root_path.size()) == 0)
    {
        //RHO_LOG("need_java_way: (2)");
        struct stat st;
        if (real_stat(fpath.c_str(), &st) == -1)
        {
            //RHO_LOG("need_java_way: (3)");
            if (errno == ENOENT)
            {
                //RHO_LOG("No such file or directory: %s, need to read it from Android package", fpath.substr(root_path.size()).c_str());
                return true;
            }

        }
        else if (S_ISREG(st.st_mode))
        {
            //RHO_LOG("need_java_way: (4)");
            rho_stat_t *rst = rho_stat(fpath);
            //RHO_LOG("need_java_way: (5)");
            if (rst && rst->mtime > st.st_mtime)
            {
                //RHO_LOG("need_java_way: %s, st.st_mtime: %lu", fpath.c_str(), st.st_mtime);
                //RHO_LOG("need_java_way: %s, rst->mtime: %lu", fpath.c_str(), rst ? rst->mtime : -1);
                //RHO_LOG("need_java_way: file %s in Android package is newer than one located on FS, unlink FS one", fpath.c_str());
                real_unlink(fpath.c_str());
                return true;
            }
        }
    }

    //RHO_LOG("need_java_way: return false");
    return false;
}

static bool need_java_way(const char *path)
{
    return path ? need_java_way(make_full_path(path)) : false;
}

RHO_GLOBAL jboolean JNICALL Java_com_rhomobile_rhodes_file_RhoFileApi_needJavaWay
  (JNIEnv *env, jclass, jstring pathObj)
{
    //RHO_LOG("Java_com_rhomobile_rhodes_file_RhoFileApi_needJavaWay");
    std::string path = rho_cast<std::string>(env, pathObj);
    //RHO_LOG("Java_com_rhomobile_rhodes_file_RhoFileApi_needJavaWay: %s", path.c_str());
    bool need = need_java_way(path);
    //RHO_LOG("Java_com_rhomobile_rhodes_file_RhoFileApi_needJavaWay: need: %d", (int)need);
    return need;
}

RHO_GLOBAL int open(const char *path, int oflag, ...)
{
    std::string fpath;
    if (path)
        fpath = path;

    if (fpath.empty())
    {
        RHO_LOG("open: path is empty");
        errno = EFAULT;
        return -1;
    }

    RHO_LOG("open: %s...", path);
    fpath = make_full_path(fpath);
    //RHO_LOG("open: %s: fpath: %s", path, fpath.c_str());

    bool java_way = need_java_way(fpath);
    if (java_way && has_pending_exception())
    {
        errno = EFAULT;
        return -1;
    }
    if (java_way && (oflag & (O_WRONLY | O_RDWR)))
    {
        //RHO_LOG("open: %s: copy from Android package", path);
        JNIEnv *env = jnienv();
        jhstring relPathObj = rho_cast<jhstring>(env, make_rel_path(fpath).c_str());
        env->CallStaticBooleanMethod(clsFileApi, midCopy, relPathObj.get());
        if (has_pending_exception())
        {
            errno = EFAULT;
            return -1;
        }

        java_way = false;
    }
    
    int fd;
    if (java_way)
    {
        JNIEnv *env = jnienv();
        jhstring relPathObj = rho_cast<jhstring>(env, make_rel_path(fpath).c_str());
        jhobject is = jhobject(env->CallStaticObjectMethod(clsFileApi, midOpen, relPathObj.get()));

        if (!is)
        {
            errno = EFAULT;
            fd = -1;
        }
        else
        {
            scoped_lock_t guard(rho_fd_mtx);
            if (!rho_fd_free.empty())
            {
                fd = rho_fd_free[0];
                rho_fd_free.erase(rho_fd_free.begin());
            }
            else
                fd = rho_fd_counter++;
            rho_fd_data_t d;
            d.is = env->NewGlobalRef(is.get());
            d.fpath = fpath;
            d.pos = 0;
            rho_fd_map[fd] = d;
        }
    }
    else
    {
        mode_t mode = 0;
        if (oflag & O_CREAT)
        {
            va_list vl;
            va_start(vl, oflag);
            mode = va_arg(vl, int);
            va_end(vl);
        }
        fd = real_open(path, oflag, mode);
    }
    RHO_LOG("open: %s => %d", path, fd);
    return fd;
}

RHO_GLOBAL int creat(const char* path, mode_t mode)
{
    return open(path, O_CREAT|O_WRONLY|O_TRUNC, mode);
}

RHO_GLOBAL int fcntl(int fd, int command, ...)
{
    RHO_LOG("fcntl: fd %d", fd);
    if (fd < RHO_FD_BASE)
    {
        va_list vl;
        va_start(vl, command);
        int arg = va_arg(vl, int);
        va_end(vl);
        return real_fcntl(fd, command, arg);
    }

    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL int close(int fd)
{
    RHO_LOG("close: fd %d", fd);
    if (fd < RHO_FD_BASE)
        return real_close(fd);

    if (has_pending_exception())
    {
        errno = EFAULT;
        return -1;
    }

    jobject is = NULL;
    {
        scoped_lock_t guard(rho_fd_mtx);

        rho_fd_map_t::iterator it = rho_fd_map.find(fd);
        if (it == rho_fd_map.end())
        {
            errno = EBADF;
            return -1;
        }

        is = it->second.is;
        rho_fd_map.erase(it);
        rho_fd_free.push_back(fd);
    }

    JNIEnv *env = jnienv();
    env->CallStaticVoidMethod(clsFileApi, midClose, is);
    env->DeleteGlobalRef(is);
    return 0;
}

RHO_GLOBAL ssize_t read(int fd, void *buf, size_t count)
{
    RHO_LOG("read: fd %d", fd);
    if (fd < RHO_FD_BASE)
    {
        ssize_t ret = real_read(fd, buf, count);
        RHO_LOG("read: fd %d: return %ld bytes (native)", fd, (long)ret);
        return ret;
    }

    if (has_pending_exception())
    {
        errno = EFAULT;
        return -1;
    }

    jobject is = NULL;
    {
        scoped_lock_t guard(rho_fd_mtx);

        rho_fd_map_t::iterator it = rho_fd_map.find(fd);
        if (it == rho_fd_map.end())
        {
            errno = EBADF;
            return (ssize_t)-1;
        }

        is = it->second.is;
    }

    JNIEnv *env = jnienv();

    jbyteArray array = env->NewByteArray(count);
    jint result = env->CallStaticIntMethod(clsFileApi, midRead, is, array);

    if (result > 0)
    {
        {
            scoped_lock_t guard(rho_fd_mtx);
            rho_fd_map_t::iterator it = rho_fd_map.find(fd);
            if (it == rho_fd_map.end())
            {
                errno = EIO;
                return (off_t)-1;
            }
            it->second.pos += result;
        }

        jbyte *arr = env->GetByteArrayElements(array, NULL);
        if (!arr)
        {
            errno = EIO;
            return (ssize_t)-1;
        }
        std::memmove(buf, arr, result);
        env->ReleaseByteArrayElements(array, arr, JNI_ABORT);
    }
    env->DeleteLocalRef(array);
    RHO_LOG("read: fd %d: return %ld bytes (java)", fd, (long)result);
    return result;
}

RHO_GLOBAL ssize_t write(int fd, const void *buf, size_t count)
{
    if (fd < RHO_FD_BASE)
        return real_write(fd, buf, count);

    errno = EBADF;
    return -1;
}

RHO_GLOBAL int access(const char *path, int mode)
{
    struct stat st;
    if (stat(path, &st) == -1)
        return -1;

    if (((mode & R_OK) && (!(st.st_mode & S_IRUSR))) ||
        ((mode & W_OK) && (!(st.st_mode & S_IWUSR))) ||
        ((mode & X_OK) && (!(st.st_mode & S_IXUSR))))
    {
        errno = EACCES;
        return -1;
    }

    return 0;
}

RHO_GLOBAL int link(const char *src, const char *dst)
{
    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL int unlink(const char *path)
{
    if (!need_java_way(path))
        return real_unlink(path);

    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL int chdir(const char *)
{
    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL int fchdir(int fd)
{
    if (fd < RHO_FD_BASE)
        return real_fchdir(fd);

    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL int rmdir(const char *)
{
    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL int chroot(const char *)
{
    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL int chown(const char *path, uid_t uid, gid_t gid)
{
    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL int fchown(int fd, uid_t uid, gid_t gid)
{
    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL int lchown(const char *path, uid_t uid, gid_t gid)
{
    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL int mknod(const char *path, mode_t mode, dev_t dev)
{
    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL int symlink(const char *src, const char *dst)
{
    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL int readlink(const char *path, char *buf, size_t bufsize)
{
    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL loff_t lseek64(int fd, loff_t offset, int whence)
{
    RHO_LOG("lseek64: fd %d, offset %llu, whence %d", fd, (unsigned long long)offset, whence);
    if (fd < RHO_FD_BASE)
    {
        loff_t ret = real_lseek64(fd, offset, whence);
        RHO_LOG("lseek64: fd %d: return %llu (native)", fd, (unsigned long long)ret);
        return ret;
    }

    if (has_pending_exception())
    {
        errno = EFAULT;
        return -1;
    }

    jobject is = NULL;
    loff_t pos = 0;

    RHO_LOG("lseek64: fd %d: java", fd);

    {
        scoped_lock_t guard(rho_fd_mtx);

        rho_fd_map_t::iterator it = rho_fd_map.find(fd);
        if (it == rho_fd_map.end())
        {
            errno = EBADF;
            return -1;
        }

        rho_stat_t *st = rho_stat(make_rel_path(it->second.fpath));
        if (!st || st->type != rho_type_file)
        {
            errno = EBADF;
            return -1;
        }

        is = it->second.is;
        pos = it->second.pos;

        switch (whence)
        {
        case SEEK_SET:
            if (offset > st->size)
                offset = st->size;
            pos = offset;
            break;
        case SEEK_CUR:
            if (pos + offset > st->size)
                offset = st->size - pos;
            pos += offset;
            break;
        case SEEK_END:
            if (offset > st->size)
                offset = st->size;
            pos = st->size - offset;
            break;
        default:
            errno = EINVAL;
            return -1;
        }

        it->second.pos = pos;
    }

    RHO_LOG("lseek64: fd %d: is: %p, pos: %llu", fd, is, (unsigned long long)pos);

    if (is == NULL)
    {
        errno = EBADF;
        return -1;
    }

    JNIEnv *env = jnienv();
    env->CallStaticVoidMethod(clsFileApi, midSeek, is, pos);

    RHO_LOG("lseek64: fd %d: return %llu (java)", fd, (unsigned long long)pos);
    return pos;
}

RHO_GLOBAL off_t lseek(int fd, off_t offset, int whence)
{
    //RHO_LOG("lseek: fd %d", fd);
    if (fd < RHO_FD_BASE)
        return real_lseek(fd, offset, whence);

    return lseek64(fd, offset, whence);
}

RHO_GLOBAL int dup(int fd)
{
    if (fd < RHO_FD_BASE)
        return real_dup(fd);

    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL int dup2(int fd, int fd2)
{
    if (fd < RHO_FD_BASE)
        return real_dup2(fd, fd2);

    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL int flock(int fd, int operation)
{
    if (fd < RHO_FD_BASE)
        return real_flock(fd, operation);

    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL int ioctl(int, int, ...)
{
    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL int fsync(int fd)
{
    if (fd < RHO_FD_BASE)
        return real_fsync(fd);

    RHO_LOG("fsync: fd %d", fd);
    errno = EINVAL;
    return -1;
}

RHO_GLOBAL int fdatasync(int fd)
{
    if (fd < RHO_FD_BASE)
        return real_fdatasync(fd);

    RHO_NOT_IMPLEMENTED;
}

RHO_GLOBAL int ftruncate(int fd, off_t offset)
{
    if (fd < RHO_FD_BASE)
        return real_ftruncate(fd, offset);

    RHO_LOG("ftruncate: fd %d", fd);
    errno = EINVAL;
    return -1;
}

static int stat_impl(std::string const &fpath, struct stat *buf)
{
    RHO_LOG("stat_impl: %s", fpath.c_str());

    if (has_pending_exception())
    {
        errno = EFAULT;
        return -1;
    }

    rho_stat_t *rst = rho_stat(fpath);
    if (!rst)
    {
        errno = ENOENT;
        return -1;
    }

    memcpy(buf, &librhodes_st, sizeof(struct stat));
    buf->st_mode = S_IRUSR|S_IWUSR;
    switch (rst->type)
    {
    case rho_type_file:
        buf->st_mode |= S_IFREG;
        break;
    case rho_type_dir:
        buf->st_mode |= S_IFDIR|S_IXUSR;
        break;
    default:
        errno = EBADF;
        return -1;
    }
    buf->st_size = rst->size;
    time_t tm = rst->mtime;
    buf->st_atime = tm;
    buf->st_mtime = tm;
    buf->st_ctime = tm;

    RHO_LOG("stat_impl: %s: size %lu, mtime: %lu", fpath.c_str(), (unsigned long)rst->size, rst->mtime);

    return 0;
}

RHO_GLOBAL int stat(const char *path, struct stat *buf)
{
    std::string fpath = make_full_path(path);
    if (!need_java_way(fpath))
        return real_stat(path, buf);

    return stat_impl(fpath, buf);
}

RHO_GLOBAL int fstat(int fd, struct stat *buf)
{
    RHO_LOG("fstat: fd %d", fd);
    if (fd < RHO_FD_BASE)
        return real_fstat(fd, buf);

    std::string fpath;

    {
        scoped_lock_t guard(rho_fd_mtx);

        rho_fd_map_t::iterator it = rho_fd_map.find(fd);
        if (it == rho_fd_map.end())
        {
            errno = EBADF;
            return -1;
        }

        fpath = it->second.fpath;
    }

    return stat_impl(fpath, buf);
}

RHO_GLOBAL int lstat(const char *path, struct stat *buf)
{
    RHO_LOG("lstat: %s", path);
    std::string fpath = make_full_path(path);
    if (!need_java_way(fpath))
        return real_lstat(path, buf);

    return stat_impl(fpath, buf);
}

static int __sread(void *cookie, char *buf, int n)
{
    RHO_LOG("__sread: %p", cookie);
    FILE *fp = (FILE*)cookie;
    int ret;

    ret = read(fp->_file, buf, n);
    if (ret >= 0)
        fp->_offset += ret;
    else
        fp->_flags &= ~__SOFF;
    return ret;
}

static int __swrite(void *cookie, const char *buf, int n)
{
    RHO_LOG("__swrite: %p", cookie);
    FILE *fp = (FILE*)cookie;

    if (fp->_flags & __SAPP)
        lseek(fp->_file, (off_t)0, SEEK_END);
    fp->_flags &= ~__SOFF;
    return write(fp->_file, buf, n);
}

static fpos_t __sseek(void *cookie, fpos_t offset, int whence)
{
    RHO_LOG("__sseek: %p", cookie);
    FILE *fp = (FILE*)cookie;
    off_t ret = lseek(fp->_file, (off_t)offset, whence);
    if (ret == (off_t)-1)
        fp->_flags &= ~__SOFF;
    else
    {
        fp->_flags |= __SOFF;
        fp->_offset = ret;
    }
    return ret;
}

static int __sclose(void *cookie)
{
    RHO_LOG("__sclose: %p", cookie);
    return close(((FILE *)cookie)->_file);
}

RHO_GLOBAL FILE *fopen(const char *path, const char *mode)
{
    RHO_LOG("fopen: %s (%s)", path, mode);

    int flags, oflags;
    if ((flags = __sflags(mode, &oflags)) == 0)
        return NULL;

    int fd = open(path, oflags, S_IRWXU);
    RHO_LOG("fopen: %s (%s): fd: %d", path, mode, fd);
    if (fd == -1)
        return NULL;

    FILE *fp = __sfp();
    if (!fp)
        return NULL;

    fp->_flags = flags;
    fp->_file = fd;
    fp->_cookie = fp;
    fp->_read = __sread;
    fp->_write = __swrite;
    fp->_seek = __sseek;
    fp->_close = __sclose;

    RHO_LOG("fopen: %s (%s): fp: %p", path, mode, fp);
    return fp;
}

RHO_GLOBAL int select(int maxfd, fd_set *rfd, fd_set *wfd, fd_set *efd, struct timeval *tv)
{
    RHO_LOG("select: maxfd: %d", maxfd);
    int count = 0;
    if (rfd)
    {
        fd_set fds;
        FD_ZERO(&fds);
        for (int i = RHO_FD_BASE; i < maxfd; ++i)
        {
            if (FD_ISSET(i, rfd))
            {
                FD_SET(i, &fds);
                ++count;
            }
        }
        if (count > 0)
        {
            RHO_LOG("select: return %d (rho)", count);
            memmove(rfd, &fds, sizeof(fds));
            if (wfd)
                FD_ZERO(wfd);
            if (efd)
                FD_ZERO(efd);
            return count;
        }
    }

    count = real_select(maxfd, rfd, wfd, efd, tv);
    RHO_LOG("select: return %d (native)", count);
    return count;
}

