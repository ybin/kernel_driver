/*
 * include/linux/err.h
 *		ERR_PTR, PTR_ERR, IS_ERR etc.
 */
 
 // kernel 中关于这些error宏的解释
 
 // 最大错误码为 4K - 1，也就是说 -MAX_ERRNO到-1，每一个都表示一种错误
 #define MAX_ERRNO 4095
 
// ERR_PTR宏可以这样理解： change "error number" to "pointer"
// 参数long error是错误码，强制类型转换为(void *)，便成了内存地址指针，如：
// error == -4095，其对应的补码表示为 0xFFFFF001
// 强制类型转换为(void *)，0xFFFFF001便成了一个地址
static inline void * __must_check ERR_PTR(long error)
{
	return (void *) error;
}

// PTR_ERR宏理解为：change "pointer" to "error number"
// 跟ERR_PTR宏正好相反
static inline long __must_check PTR_ERR(const void *ptr)
{
	return (long) ptr;
}

// IS_ERR宏的原理是这样的：
//		内核分配的内存都是以page为单位的，在32位系统上，一个page为4K大小，
//		但是最后一个page是禁止使用的，看高端内存可知，这最后一个page是作为
//		fixed_map使用的，像驱动程序等是无法分配到这个page的，所以如果分配的
//		内存指针跑到最后一个page地址范围(0xFFFFF000 ~ 0xFFFFFFFF)了，那么
//		这个一定是发生错误了。
static inline long __must_check IS_ERR(const void *ptr)
{
	return IS_ERR_VALUE((unsigned long)ptr);
}

#define IS_ERR_VALUE(x) unlikely((x) >= (unsigned long)-MAX_ERRNO)
// 其中(unsighed long)-MAX_ERRNO的十六进制表示为：0xFFFFF001，恰好比最后一个
// page的地址(0xFFFFF000)大 1.
// 可以联合这些宏来使用：
//		如果IS_ERR宏返回true，就调用PTR_ERR宏得到错误码。
// 这就是PTR_RET宏的实现原理：
static inline int __must_check PTR_RET(const void *ptr)
{
	if (IS_ERR(ptr))
		return PTR_ERR(ptr);
	else
		return 0;
}
