#include "stdafx.h"
#include "Utils.h"

uint32_t Is2n(uint32_t un)
{
    return un&(un - 1);
}

uint32_t Max2n(uint32_t un)
{
    uint32_t mi = Is2n(un);
    return mi ? Max2n(mi) : un;
}

uint32_t Max2nUp(uint32_t un)
{
    return Is2n(un) ? Max2n(un) * 2 : un;
}

void LogError(char* pMsg, ...)
{
    printf("[ERROR] - ");
    va_list _ArgList;
    __crt_va_start(_ArgList, pMsg);
    _vfprintf_l(stdout, pMsg, NULL, _ArgList);
    __crt_va_end(_ArgList);
    printf("\n");
    return;
}

void LogInfo(char* pMsg, ...)
{
    printf("[INFO] - ");
    va_list _ArgList;
    __crt_va_start(_ArgList, pMsg);
    _vfprintf_l(stdout, pMsg, NULL, _ArgList);
    __crt_va_end(_ArgList);
    printf("\n");
    return;
}
