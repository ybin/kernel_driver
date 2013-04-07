/*
 * HAL Implementation
 *	--- take Qualcomm for example.
 */

/*
 * ����qualcomm 8960ƽ̨��HALʵ�֣��ò��ִ��벢��**private**�ģ�Google��վ�������ص���
 *
 * ����˼·���£�
 * 	1. ������ΪHMI��hw_module_t����libhardware��load HAL��.so�ļ�ʱ����Ҹö���
 *	2. �����µ��������ͣ�camera_hardware_t��������һ��camera device�������ṩ
 *		camera_device_t���󣬶��±���QCameraHardwareInterface����ͨ���ö�����ɾ��幦��
 *	���д����λ�ڣ�[@hardware/libhardware/include/hardware/QualcommCamera2.cpp]
 */
 
// һ������hw_module_t����HMI������ʼ��
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
	// ����Ϊα���룡
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
	
	// ����hw_device_t����
	*hw_device = (hw_device_t *)&device->common;
}
// ������camera����������HAL�ӿ�ʵ�� [@hardware/qcom/camera/QualcommCamera2.cpp]
extern "C" int get_number_of_cameras() {
	return android::HAL_getNumberOfCameras();
}

extern "C" int get_camera_info(int camera_id, struct camera_info *info) {
}

extern "C" int HAL_getNumberOfCameras() // [@hardware/qcom/camera/QCameraHAL.cpp]
{
	
}

// ����ʹ��camera_hardware_t����һ��camera device�����а��������ṩ��camera_device_t����
// 		�Ͷ��µ��õ�QCameraHardwareInterface����
typedef struct {
	camera_device hw_dev;	// framework��ͨ��module�����ȡ��device����
	QCameraHardwareInterface *hardware;	// ����Ĺ���Ҫͨ���ö�������ɣ������һ�����뵽kernel��
	int camera_released;
	int cameraId;
} camera_hardware_t;
/*
 * ������ȫ���԰��Լ����Ϊ"��������"����libhardware�ṩ�Ľӿ�struct camera_device������linux kernel�ṩ��
 * struct file�ӿڣ�������Ҫ�ṩstruct camera_device�е�struct camera_device_ops�еĸ��������ӿڡ�
 * �˴���camera_hardware_t�Ǹ�ͨΪ�˱����Լ��ĳ���״̬��˽�����ݶ������ģ�����������Լ�ʵ��һ���򵥵�HAL��
 * ��ȫ���Բ��ô������Ƶ����ݽṹ����ֻ���ṩstruct camera_device_ops�еĸ��������ӿ�ʵ�֣������Ǿ仰��
 *		"������linux device driver"
 */
camera_device_ops_t camera_ops = {
	// ...
	start_preview: android::start_preview,
	// ...
	take_picture: android::take_picture,
	// ...
};
/*
 * ������take_picture()����Ϊ��������ͨ��struct camera_device�����ȡ��QCameraHardwareInterface����
 * ����QCameraHardwareInterface�����ǰ�����camera_hardware_t�����еģ�����ȴͨ��camera_device�����ȡ��
 * ��ô�������أ�
 * ��ʵ���Ա���������Ŀ�ܾͲ�������ˣ�
 *		struct file		<=>		struct camera_device
 *		struct scull	<=>		struct camera_hardware_t
 * �����������������ȫ�Ǻϣ�����cdev����֪���ˣ������Ҫͼ�������͡�
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

//************* �������HAL��kernel�������� ***************//