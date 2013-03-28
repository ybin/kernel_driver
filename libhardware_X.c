/*
 * libhardware.so - 完成framework与HAL的对接
 */

/*
 * 该.so文件的内容比较简单，主要完成两件事情：
 *	1. 定义各硬件的统一接口，类似于硬件对象的"基类"
 *		由于有些硬件类型可能有多个硬件实例，比如camera就有两个，所以具体实现上又分为module与device，
 *		module对应硬件类型，device对应具体的硬件实例，通过module对象可以取得具体的device对象，比如：
 *			module : camera
 *				device-0 : back-camera
 *				device-1 : front-camera
 *	2. 实现framework获取硬件对象的接口，通过该接口，framework取得硬件的module对象
 *	   然后通过module对象的methods取得具体的device对象。至于后续如何使用device，那就是各个module自己的事情了
 *		a. int hw_get_module(const char *id, const struct hw_module_t **module);
 *		b. int hw_get_module_by_class(const char *class_id, const char *inst,
 *										const struct hw_module_t **module);
 */
#define HAL_MODULE_INFO_SYM			HMI
#define HAL_MODULE_INFO_SYM_AS_STR	"HMI"
 
// 一、定义module、device的统一接口(基类)[@hardware/libhardware/include/hardware/hardware.h]
/*
 * 每个hardware module必须提供一个名为HAL_MODULE_INFO_SYM的数据结构，并且该结构必须以hw_module_t开始，
 * 后面跟随该module自己特殊的信息。这是C语言中对面向对象的模拟。
 */
typedef struct hw_module_t {
	uint32_t tag;
	uint16_t version_major;
	uint16_t version_minor;
	const char *id; // 用于hw_get_module()函数中的id参数
	const char *name;
	const char *author;
	struct hw_module_methods_t *methods;
	void *dso;	// 通过dlopen打开.so文件时返回的handle(操作.so文件的句柄)
	uint32_t reserved[32-7];
} hw_module_t;

typedef struct hw_module_methods_t {
	// 唯一open函数就是为了取得hw_device_t对象
	int (*open)(const struct hw_module_t *module, const char *id,
					struct hw_device_t **device);
} hw_module_methods_t;

typedef struct hw_device_t {
	uint32_t tag;
	uint32_t version;
	struct hw_module_t *module;	// 该device属于哪个module呢
	uint32_t reserved[12];
	int (*close)(struct hw_device_t *device);	// 我们必须提供关闭设备的接口
} hw_device_t;

// 二、如何取得module对象 - 获取module对象的两个接口是如何实现的[@hardware/libhardware/hardware.c]
/** Base path of the hal modules */
#define HAL_LIBRARY_PATH1 "/system/lib/hw"
#define HAL_LIBRARY_PATH2 "/vendor/lib/hw"

/*
 * .so文件的命名遵循一个简单的规则：<MODULE_ID>.variant.so，比如Camera.msm7k.so
 */
// 这里的variant_keys包含的就是文件名中的"variant"部分
static const char *variant_keys[] = {
    "ro.hardware",  /* This goes first so that it can pick up a different
                       file on the emulator. */
    "ro.product.board",
    "ro.board.platform",
    "ro.arch"
};
// 共有多少种variant呢
static const int HAL_VARIANT_KEYS_COUNT =
    (sizeof(variant_keys)/sizeof(variant_keys[0]));

/*
 * 利用dlopen加载.so文件(如Camera.msm7k.so), 然后利用dlsym找到module对象的引用并返回(**pHmi)
 */
static int load(const char *id,
        const char *path,
        const struct hw_module_t **pHmi)
{
    int status;
    void *handle;
    struct hw_module_t *hmi;

    /*
     * load the symbols resolving undefined symbols before
     * dlopen returns. Since RTLD_GLOBAL is not or'd in with
     * RTLD_NOW the external symbols will not be global
     */
    handle = dlopen(path, RTLD_NOW);
    if (handle == NULL) {
        char const *err_str = dlerror();
        ALOGE("load: module=%s\n%s", path, err_str?err_str:"unknown");
        status = -EINVAL;
        goto done;
    }

    /* Get the address of the struct hal_module_info. */
    const char *sym = HAL_MODULE_INFO_SYM_AS_STR;
    hmi = (struct hw_module_t *)dlsym(handle, sym);
    if (hmi == NULL) {
        ALOGE("load: couldn't find symbol %s", sym);
        status = -EINVAL;
        goto done;
    }

    /* Check that the id matches */
    if (strcmp(id, hmi->id) != 0) {
        ALOGE("load: id=%s != hmi->id=%s", id, hmi->id);
        status = -EINVAL;
        goto done;
    }

    hmi->dso = handle;

    /* success */
    status = 0;

    done:
    if (status != 0) {
        hmi = NULL;
        if (handle != NULL) {
            dlclose(handle);
            handle = NULL;
        }
    } else {
        ALOGV("loaded HAL id=%s path=%s hmi=%p handle=%p",
                id, path, *pHmi, handle);
    }

    *pHmi = hmi;

    return status;
}
// 获取硬件的module对象(通过参数列表中的**module返回)
int hw_get_module_by_class(const char *class_id, const char *inst,
                           const struct hw_module_t **module)
{
    int status;
    int i;
    const struct hw_module_t *hmi = NULL;
	// PATH_MAX 是kernel中定义的宏，它等于4096(包括结尾的\0)
    char prop[PATH_MAX];
    char path[PATH_MAX];
    char name[PATH_MAX];
	// 参数const char *inst只在此处使用，是干啥的呢？
    if (inst)
        snprintf(name, PATH_MAX, "%s.%s", class_id, inst);
    else
        strlcpy(name, class_id, PATH_MAX);

    /*
     * Here we rely on the fact that calling dlopen multiple times on
     * the same .so will simply increment a refcount (and not load
     * a new copy of the library).
     * We also assume that dlopen() is thread-safe.
     */
	// 首先遍历检查vendor/lib/hw/目录下的.so文件，如果存在就使用它，如果不存在就
	// 检查system/lib/hw/目录下的.so文件，如果存在就是用它，如果不存在就使用
	// system/lib/hw/<MODULE_ID>.default.so
    /* Loop through the configuration variants looking for a module */
    for (i=0 ; i<HAL_VARIANT_KEYS_COUNT+1 ; i++) {
        if (i < HAL_VARIANT_KEYS_COUNT) {
            if (property_get(variant_keys[i], prop, NULL) == 0) {
                continue;
            }
            snprintf(path, sizeof(path), "%s/%s.%s.so",
                     HAL_LIBRARY_PATH2, name, prop);
            if (access(path, R_OK) == 0) break;

            snprintf(path, sizeof(path), "%s/%s.%s.so",
                     HAL_LIBRARY_PATH1, name, prop);
            if (access(path, R_OK) == 0) break;
        } else {
            snprintf(path, sizeof(path), "%s/%s.default.so",
                     HAL_LIBRARY_PATH1, name);
            if (access(path, R_OK) == 0) break;
        }
    }

    status = -ENOENT;
	// 开始load .so文件
    if (i < HAL_VARIANT_KEYS_COUNT+1) {
        /* load the module, if this fails, we're doomed, and we should not try
         * to load a different variant. */
        status = load(class_id, path, module);
    }

    return status;
}
// 一个空壳函数，主要实现在hw_get_module_by_class()中
int hw_get_module(const char *id, const struct hw_module_t **module)
{
    return hw_get_module_by_class(id, NULL, module);
}


//****************** 以上是 libhardware 的通用定义，下面看camera模块的定义，都是android的标准定义 ******************
[@hardware/libhardware/include/hardware/camera.h]
struct camera_info {
    int facing;
    int orientation;
};
// 从 C语言模拟面向对象的观点来看， camera_module_t可以看做是hw_module_t的子类
typedef struct camera_module {
    hw_module_t common;
	// 下面是子类扩展出来的接口
    int (*get_number_of_cameras)(void);
    int (*get_camera_info)(int camera_id, struct camera_info *info);
} camera_module_t;
// 从 C语言模拟面向对象的观点来看， camera_device_t可以看做是hw_device_t的子类
typedef struct camera_device {
    hw_device_t common;
	// 下面是子类扩展出来的接口及属性，camera的功能主要在camera_device_ops_t对象中实现
    camera_device_ops_t *ops;
    void *priv;
} camera_device_t;
// 这里面的接口是不是很眼熟的呢?! 在framework的角度看底层，只能看到这些功能。
typedef struct camera_device_ops {
    /** Set the ANativeWindow to which preview frames are sent */
    int (*set_preview_window)(struct camera_device *,
            struct preview_stream_ops *window);

    /** Set the notification and data callbacks */
    void (*set_callbacks)(struct camera_device *,
            camera_notify_callback notify_cb,
            camera_data_callback data_cb,
            camera_data_timestamp_callback data_cb_timestamp,
            camera_request_memory get_memory,
            void *user);

    /**
     * The following three functions all take a msg_type, which is a bitmask of
     * the messages defined in include/ui/Camera.h
     */

    /**
     * Enable a message, or set of messages.
     */
    void (*enable_msg_type)(struct camera_device *, int32_t msg_type);

    /**
     * Disable a message, or a set of messages.
     *
     * Once received a call to disableMsgType(CAMERA_MSG_VIDEO_FRAME), camera
     * HAL should not rely on its client to call releaseRecordingFrame() to
     * release video recording frames sent out by the cameral HAL before and
     * after the disableMsgType(CAMERA_MSG_VIDEO_FRAME) call. Camera HAL
     * clients must not modify/access any video recording frame after calling
     * disableMsgType(CAMERA_MSG_VIDEO_FRAME).
     */
    void (*disable_msg_type)(struct camera_device *, int32_t msg_type);

    /**
     * Query whether a message, or a set of messages, is enabled.  Note that
     * this is operates as an AND, if any of the messages queried are off, this
     * will return false.
     */
    int (*msg_type_enabled)(struct camera_device *, int32_t msg_type);

    /**
     * Start preview mode.
     */
    int (*start_preview)(struct camera_device *);

    /**
     * Stop a previously started preview.
     */
    void (*stop_preview)(struct camera_device *);

    /**
     * Returns true if preview is enabled.
     */
    int (*preview_enabled)(struct camera_device *);

    /**
     * Request the camera HAL to store meta data or real YUV data in the video
     * buffers sent out via CAMERA_MSG_VIDEO_FRAME for a recording session. If
     * it is not called, the default camera HAL behavior is to store real YUV
     * data in the video buffers.
     *
     * This method should be called before startRecording() in order to be
     * effective.
     *
     * If meta data is stored in the video buffers, it is up to the receiver of
     * the video buffers to interpret the contents and to find the actual frame
     * data with the help of the meta data in the buffer. How this is done is
     * outside of the scope of this method.
     *
     * Some camera HALs may not support storing meta data in the video buffers,
     * but all camera HALs should support storing real YUV data in the video
     * buffers. If the camera HAL does not support storing the meta data in the
     * video buffers when it is requested to do do, INVALID_OPERATION must be
     * returned. It is very useful for the camera HAL to pass meta data rather
     * than the actual frame data directly to the video encoder, since the
     * amount of the uncompressed frame data can be very large if video size is
     * large.
     *
     * @param enable if true to instruct the camera HAL to store
     *        meta data in the video buffers; false to instruct
     *        the camera HAL to store real YUV data in the video
     *        buffers.
     *
     * @return OK on success.
     */
    int (*store_meta_data_in_buffers)(struct camera_device *, int enable);

    /**
     * Start record mode. When a record image is available, a
     * CAMERA_MSG_VIDEO_FRAME message is sent with the corresponding
     * frame. Every record frame must be released by a camera HAL client via
     * releaseRecordingFrame() before the client calls
     * disableMsgType(CAMERA_MSG_VIDEO_FRAME). After the client calls
     * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
     * responsibility to manage the life-cycle of the video recording frames,
     * and the client must not modify/access any video recording frames.
     */
    int (*start_recording)(struct camera_device *);

    /**
     * Stop a previously started recording.
     */
    void (*stop_recording)(struct camera_device *);

    /**
     * Returns true if recording is enabled.
     */
    int (*recording_enabled)(struct camera_device *);

    /**
     * Release a record frame previously returned by CAMERA_MSG_VIDEO_FRAME.
     *
     * It is camera HAL client's responsibility to release video recording
     * frames sent out by the camera HAL before the camera HAL receives a call
     * to disableMsgType(CAMERA_MSG_VIDEO_FRAME). After it receives the call to
     * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
     * responsibility to manage the life-cycle of the video recording frames.
     */
    void (*release_recording_frame)(struct camera_device *,
                    const void *opaque);

    /**
     * Start auto focus, the notification callback routine is called with
     * CAMERA_MSG_FOCUS once when focusing is complete. autoFocus() will be
     * called again if another auto focus is needed.
     */
    int (*auto_focus)(struct camera_device *);

    /**
     * Cancels auto-focus function. If the auto-focus is still in progress,
     * this function will cancel it. Whether the auto-focus is in progress or
     * not, this function will return the focus position to the default.  If
     * the camera does not support auto-focus, this is a no-op.
     */
    int (*cancel_auto_focus)(struct camera_device *);

    /**
     * Take a picture.
     */
    int (*take_picture)(struct camera_device *);

    /**
     * Cancel a picture that was started with takePicture. Calling this method
     * when no picture is being taken is a no-op.
     */
    int (*cancel_picture)(struct camera_device *);

    /**
     * Set the camera parameters. This returns BAD_VALUE if any parameter is
     * invalid or not supported.
     */
    int (*set_parameters)(struct camera_device *, const char *parms);

    /** Retrieve the camera parameters.  The buffer returned by the camera HAL
        must be returned back to it with put_parameters, if put_parameters
        is not NULL.
     */
    char *(*get_parameters)(struct camera_device *);

    /** The camera HAL uses its own memory to pass us the parameters when we
        call get_parameters.  Use this function to return the memory back to
        the camera HAL, if put_parameters is not NULL.  If put_parameters
        is NULL, then you have to use free() to release the memory.
    */
    void (*put_parameters)(struct camera_device *, char *);

    /**
     * Send command to camera driver.
     */
    int (*send_command)(struct camera_device *,
                int32_t cmd, int32_t arg1, int32_t arg2);

    /**
     * Release the hardware resources owned by this object.  Note that this is
     * *not* done in the destructor.
     */
    void (*release)(struct camera_device *);

    /**
     * Dump state of the camera hardware
     */
    int (*dump)(struct camera_device *, int fd);
} camera_device_ops_t;
