

// 0. SYSCALL_DEFINE3 macro [@include/linux/syscalls.h]
#define SYSCALL_DEFINE3(name, ...) SYSCALL_DEFINEx(3, _##name, __VA_ARGS__)

// 1. SYSCALL_DEFINEx [@include/linux/syscalls.h]
#define SYSCALL_DEFINEx(x, sname, ...) __SYSCALL_DEFINEx(x, sname, __VA_ARGS__)

// 2. __SYSCALL_DEFINEx [@include/linux/syscalls.h]
#define __SYSCALL_DEFINEx(x, name, ...)					\
	asmlinkage long sys##name(__SC_DECL##x(__VA_ARGS__))

// 3. __SC_DECL3 [@include/linux/syscalls.h]
#define __SC_DECL1(t1, a1)	t1 a1
#define __SC_DECL2(t2, a2, ...) t2 a2, __SC_DECL1(__VA_ARGS__)
#define __SC_DECL3(t3, a3, ...) t3 a3, __SC_DECL2(__VA_ARGS__)
#define __SC_DECL4(t4, a4, ...) t4 a4, __SC_DECL3(__VA_ARGS__)
#define __SC_DECL5(t5, a5, ...) t5 a5, __SC_DECL4(__VA_ARGS__)
#define __SC_DECL6(t6, a6, ...) t6 a6, __SC_DECL5(__VA_ARGS__)

// 4. e.g. 
//			SYSCALL_DEFINE3(open, const char __user *, filename, int, flags, umode_t, mode)
//		最后展开为:
//			long sys_open(const char _user * filename, int flags, umode_t mode)
//		所以 SYSCALL_DEFINE3 中的 3 代表 3 个参数.

// P.S.
#define debug(...) printf(__VA_ARGS__)
// 缺省号代表一个可以变化的参数表。使用保留名 __VA_ARGS__ 把参数传递给宏。当宏的调用展开时，实际的参数就传递给 printf()了。例如: 
// Debug("Y = %d\n", y);
// 而处理器会把宏的调用替换成: 
printf("Y = %d\n", y);
