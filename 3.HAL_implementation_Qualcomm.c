/*
 * HAL Implementation
 *	--- take Qualcomm for example.
 */

/*
 * 这是qualcomm 8960平台的HAL实现，该部分代码并非**private**的，Google网站可以下载到！
 *
 * 总体思路如下：
 * 	1. 创建名为HMI的hw_module_t对象，libhardware在load HAL的.so文件时会查找该对象
 *	2. 创建新的数据类型：camera_hardware_t，它代表一个camera device，对上提供
 *		camera_device_t对象，对下保存QCameraHardwareInterface对象，通过该对象完成具体功能
 *	所有代码均位于：[@hardware/libhardware/include/hardware/QualcommCamera2.cpp]
 */
 
// 一、创建hw_module_t对象：HMI，并初始化
camera_module_t HAL_MODULE_INFO_SYM = {
	common: camera_common,
	get_number_of_cameras: get_number_of_cameras,
	get_camera_info: get_camera_info,
};

static hw_module_t camera_common = {
	tag: HARDWARE_MODULE_TAG,
	version_major: 0,
	version_minor: 01,
	id: CAMERA_HARDWARE_MODULE_ID,  // equals "camera"
	name: "Qcamera",
	author: "Qcom",
	methods: &camera_module_methods,
	dso: NULL,
};

static hw_module_methods_t camera_module_methods = {
	open: camera_device_open,
};

extern "C" int camera_device_open(const struct hw_module_t *module,
									const char *id, struct hw_device_t **hw_device) {
	// 以下为伪代码！
	camera_device *device = NULL;
	// 0. create camera_hardware_t object
	camera_hardware_t *camHal = (camera_hardware_t *)malloc(sizeof(camera_hardware_t));
	memset(camHal, 0, sizeof(camera_hardware_t));
	// 1. set QCameraHardwareInterface object
	camHal->hardware = new QCameraHardwareInterface(cameraId);
	// 2. set camera_device_t object
	device = &camHal->hw_dev;
	device->common.close = close_camera_device;
	device->ops = &camera_ops;
	device->priv = (void *)camHal;
	
	// 返回hw_device_t对象
	*hw_device = (hw_device_t *)&device->common;
}
// 以下是camera的两个特殊HAL接口实现 [@hardware/qcom/camera/QualcommCamera2.cpp]
extern "C" int get_number_of_cameras() {
	return android::HAL_getNumberOfCameras();
}

extern "C" int get_camera_info(int camera_id, struct camera_info *info) {
}

extern "C" int HAL_getNumberOfCameras() // [@hardware/qcom/camera/QCameraHAL.cpp]
{
	
}

// 二、使用camera_hardware_t代表一个camera device，其中包括对上提供的camera_device_t对象
// 		和对下调用的QCameraHardwareInterface对象
typedef struct {
	camera_device hw_dev;	// framework会通过module对象获取该device对象
	QCameraHardwareInterface *hardware;	// 具体的功能要通过该对象来完成，它会进一步深入到kernel中
	int camera_released;
	int cameraId;
} camera_hardware_t;
/*
 * 我们完全可以把自己理解为"驱动程序"，而libhardware提供的接口struct camera_device类似于linux kernel提供的
 * struct file接口，我们需要提供struct camera_device中的struct camera_device_ops中的各个函数接口。
 * 此处的camera_hardware_t是高通为了保存自己的程序状态和私有数据而创建的，如果让我们自己实现一个简单的HAL，
 * 完全可以不用创建类似的数据结构，而只需提供struct camera_device_ops中的各个函数接口实现，还是那句话：
 *		"类似于linux device driver"
 */
camera_device_ops_t camera_ops = {
	// ...
	start_preview: android::start_preview,
	// ...
	take_picture: android::take_picture,
	// ...
};
/*
 * 这里以take_picture()函数为例，它会通过struct camera_device对象获取到QCameraHardwareInterface对象，
 * 可是QCameraHardwareInterface对象是包含在camera_hardware_t对象中的，这里却通过camera_device对象获取，
 * 怎么做到的呢？
 * 其实，对比驱动程序的框架就不难理解了：
 *		struct file		<=>		struct camera_device
 *		struct scull	<=>		struct camera_hardware_t
 * 但是这个比喻并不完全吻合，考虑cdev，就知道了，这个需要图表来解释。
 * util_get_Hal_obj():
 * 	camera_device_t -> camera_hardware_t -> QCameraHardwareInterface
 */
int take_picture(struc camera_device *device) {
	int rc = -1;
	QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
	if(hardware != NULL) {
		rc = hardware->takePicture();
	}
	return rc;
}

//************* 后面便是HAL到kernel的内容了 ***************//