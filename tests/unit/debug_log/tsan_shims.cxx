// TSan visibility shims (test harness only, not flrig code).
// On macOS, TSan does not intercept snprintf/vsnprintf, so writes into the
// static buffers fmt/sztemp in debug.cxx would be invisible.  These wrappers
// override the libc symbols for this executable, tell TSan about the buffer
// write and the format-string read, then call the real libc function.
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
extern "C" void __tsan_read_range(void *addr, unsigned long size);
extern "C" void __tsan_write_range(void *addr, unsigned long size);
typedef int (*vsnp_t)(char *, size_t, const char *, va_list);
static vsnp_t real_vsnprintf()
{
	static vsnp_t p = (vsnp_t)dlsym(RTLD_NEXT, "vsnprintf");
	return p;
}
extern "C" int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap)
{
	__tsan_read_range((void *)fmt, strlen(fmt) + 1);
	if (n) __tsan_write_range(buf, n);
	return real_vsnprintf()(buf, n, fmt, ap);
}
extern "C" int snprintf(char *buf, size_t n, const char *fmt, ...)
{
	va_list ap; va_start(ap, fmt);
	int r = vsnprintf(buf, n, fmt, ap);
	va_end(ap);
	return r;
}
