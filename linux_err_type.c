/*
 * include/linux/err.h
 *		ERR_PTR, PTR_ERR, IS_ERR etc.
 */
 
 // kernel �й�����Щerror��Ľ���
 
 // ��������Ϊ 4K - 1��Ҳ����˵ -MAX_ERRNO��-1��ÿһ������ʾһ�ִ���
 #define MAX_ERRNO 4095
 
// ERR_PTR�����������⣺ change "error number" to "pointer"
// ����long error�Ǵ����룬ǿ������ת��Ϊ(void *)��������ڴ��ַָ�룬�磺
// error == -4095�����Ӧ�Ĳ����ʾΪ 0xFFFFF001
// ǿ������ת��Ϊ(void *)��0xFFFFF001�����һ����ַ
static inline void * __must_check ERR_PTR(long error)
{
	return (void *) error;
}

// PTR_ERR�����Ϊ��change "pointer" to "error number"
// ��ERR_PTR�������෴
static inline long __must_check PTR_ERR(const void *ptr)
{
	return (long) ptr;
}

// IS_ERR���ԭ���������ģ�
//		�ں˷�����ڴ涼����pageΪ��λ�ģ���32λϵͳ�ϣ�һ��pageΪ4K��С��
//		�������һ��page�ǽ�ֹʹ�õģ����߶��ڴ��֪�������һ��page����Ϊ
//		fixed_mapʹ�õģ���������������޷����䵽���page�ģ�������������
//		�ڴ�ָ���ܵ����һ��page��ַ��Χ(0xFFFFF000 ~ 0xFFFFFFFF)�ˣ���ô
//		���һ���Ƿ��������ˡ�
static inline long __must_check IS_ERR(const void *ptr)
{
	return IS_ERR_VALUE((unsigned long)ptr);
}

#define IS_ERR_VALUE(x) unlikely((x) >= (unsigned long)-MAX_ERRNO)
// ����(unsighed long)-MAX_ERRNO��ʮ�����Ʊ�ʾΪ��0xFFFFF001��ǡ�ñ����һ��
// page�ĵ�ַ(0xFFFFF000)�� 1.
// ����������Щ����ʹ�ã�
//		���IS_ERR�귵��true���͵���PTR_ERR��õ������롣
// �����PTR_RET���ʵ��ԭ��
static inline int __must_check PTR_RET(const void *ptr)
{
	if (IS_ERR(ptr))
		return PTR_ERR(ptr);
	else
		return 0;
}
