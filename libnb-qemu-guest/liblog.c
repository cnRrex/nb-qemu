#include <stdio.h>

int __android_log_print(int prio, const char *tag,  const char *fmt, ...)
{
    int ret;
    va_list args;

    printf("[libnb-qemu guest log]/[%s]: ", tag);

    va_start(args, fmt);
    ret = vprintf(fmt, args);
    va_end(args);

    putc('\n', stdout);

    return ret;
}

int __system_property_get(const char* __name, char* __value)
{
    return -1;
}

