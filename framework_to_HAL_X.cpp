/*
 * How framework connect to HAL
 *	--- take Camera for example
 */

/*
 * framework连接HAL可以分为两个大的步骤：
 *	1. 获取camera_module_t对象：mModule
 *		从hw_module_t及camera_module_t两个struct的定义，结合C语言的模拟面向对象用法来看，
 *		camera_module_t可以看做是hw_module_t的子类
 *	2. 通过mModule对象获取camera_device_t类型的对象：mDevice
 */
 
// 一、获取camera_module_t对象：mModule [@framework/av/services/camera/libcameraservice/CameraService.cpp]
// camera service在系统启动时实例化，实例化之初便执行到onFirstRef()函数，在这里，我们取得了camera module对象：
void CameraService::onFirstRef()
{
    BnCameraService::onFirstRef();

	// [@hardware/libhardware/include/hardware/camera.h]
	// #define CAMERA_HARDWARE_MODULE_ID   "camera"
	// hw_get_module()的内容，见libhardware的分析，是Android标准的HAL接口
    if (hw_get_module(CAMERA_HARDWARE_MODULE_ID,
                (const hw_module_t **)&mModule) < 0) {
        ALOGE("Could not load camera HAL module");
        mNumberOfCameras = 0;
    }
    else {
		// get_number_of_cameras()便是camera_module_t这个"子类"扩展
		// hw_module_t这个"父类"的接口时，增加的其中一个。
        mNumberOfCameras = mModule->get_number_of_cameras();
        if (mNumberOfCameras > MAX_CAMERAS) {
            ALOGE("Number of cameras(%d) > MAX_CAMERAS(%d).",
                    mNumberOfCameras, MAX_CAMERAS);
            mNumberOfCameras = MAX_CAMERAS;
        }
        for (int i = 0; i < mNumberOfCameras; i++) {
            setCameraFree(i);
        }
    }
}

// 二、通过mModule对象获取camera_device_t类型的对象：mDevice 
// [@framework/av/services/camera/libcameraservice/CameraService.cpp::connect()]
/*
 * client端连接到server端时，会调用的connect()函数，该函数中会实例化一个Client对象，
 * 来为client服务，之后便是client <=> Client对象进行通信，注意这两者是位于两个进程的，
 * 它们通过binder进行通信。
 */
sp<ICamera> CameraService::connect(
        const sp<ICameraClient>& cameraClient, int cameraId) {
    int callingPid = getCallingPid();
    sp<CameraHardwareInterface> hardware = NULL;

	// ......

	// camera_device_name其实就是一个类似"0", "1"之类的字符串
	char camera_device_name[10];
    snprintf(camera_device_name, sizeof(camera_device_name), "%d", cameraId);
	// 创建CameraHardwareInterface类型的对象，Client对象的各个接口，如takePicture()，
	// 都是调用该对象(hardware, 在Client类的定义中，这个对象叫做mHardware)来完成的
    hardware = new CameraHardwareInterface(camera_device_name);
	// *** 下面的初始化是关键，我们的mDevice就是在这里获取到的
	// *** 根据<一>的讲解，mModule->common其实就是camera_module_t对象中包含的
	// *** hw_module_t，再根据libhardware的讲解，hw_module_t中有一个标准接口：
	// *** 		hw_module_methods_t
	// *** 其中包含一个open()函数，我们的mDevice就是通过open()函数取到的。
    if (hardware->initialize(&mModule->common) != OK) {
        hardware.clear();
        return NULL;
    }
	// 构建Client对象，以后该对象直接跟client端的代理cameraClient进行通信
    client = new Client(this, cameraClient, hardware, cameraId, info.facing, callingPid);
    mClient[cameraId] = client;
    LOG1("CameraService::connect X (id %d)", cameraId);
    return client;
}

//  [@framework/av/services/camera/libcameraservice/CameraService.h::CameraHardwareInterface]
status_t initialize(hw_module_t *module)
{
	ALOGI("Opening camera %s", mName.string());
	// 在此处，我们取到了camera_device_t对象：mDevice
	int rc = module->methods->open(module, mName.string(),
								   (hw_device_t **)&mDevice);
	if (rc != OK) {
		ALOGE("Could not open camera %s: %d", mName.string(), rc);
		return rc;
	}
	initHalPreviewWindow();
	return rc;
}

// 至于mModule, mDevice的具体内容是什么、mDevice对象是如何通过open()函数取得的，
// 这些就要看不同硬件厂商自己的定义了，我们另作分析。

