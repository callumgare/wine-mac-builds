/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Timeout: 30
 * Env: WINENCPU=2
 * Wrapper: mac-cpus
 * Guards: ntdll-cpu-count-cap
 * Fails without on: wine-macfixes-11.22-build0, macOS 26.7.2, M1 Max, 2026-11-26
 *
 * Some games break on Macs with many cores; WINENCPU=<n> makes Wine report
 * at most n processors. Every way of asking has to agree, or a game that
 * sizes its thread pool from one and pins threads from another goes wrong.
 *
 * Passes when, with WINENCPU=2, GetSystemInfo, GetActiveProcessorCount,
 * GetLogicalProcessorInformation and GetLogicalProcessorInformationEx all
 * report 2 logical processors, the process affinity mask is 0x3, and
 * NtQuerySystemInformation(SystemProcessorPerformanceInformation) returns 2
 * entries even when its buffer has room for more.
 * Skipped on a Mac with 2 processors or fewer, where there's nothing to
 * cap. (The real count comes from the mac-cpus helper: inside Wine every
 * count, the registry's included, is the capped one.)
 */

#include <windows.h>
#include <stdio.h>

static int popcount(ULONG_PTR mask)
{
    int n = 0;

    for (; mask; mask >>= 1) n += mask & 1;
    return n;
}

int main(void)
{
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION *info;
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *ex;
    DWORD len = 0, i, real;
    DWORD_PTR proc_mask, sys_mask;
    SYSTEM_INFO si;
    int ok = 1, n;
    char *p, buf[16];

    GetSystemInfo(&si);

    real = GetEnvironmentVariableA("WMB_MAC_CPUS", buf, sizeof(buf)) ? atoi(buf) : 0;
    printf("winencpu: the Mac has %lu processors\n", real);
    if (!real)
    {
        printf("winencpu: WMB_MAC_CPUS isn't set (the mac-cpus helper sets it)\n");
        return 1;
    }
    if (real <= 2)
    {
        printf("winencpu: skipped: this Mac has no more than 2 processors to cap\n");
        return 77;
    }

    printf("winencpu: GetSystemInfo: %lu processors, mask %#lx\n", si.dwNumberOfProcessors,
           (unsigned long)si.dwActiveProcessorMask);
    if (si.dwNumberOfProcessors != 2) ok = 0;
    if (popcount(si.dwActiveProcessorMask) != 2) ok = 0;

    n = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    printf("winencpu: GetActiveProcessorCount: %d\n", n);
    if (n != 2) ok = 0;

    if (GetProcessAffinityMask(GetCurrentProcess(), &proc_mask, &sys_mask))
    {
        printf("winencpu: process affinity %#lx, system %#lx\n", (unsigned long)proc_mask, (unsigned long)sys_mask);
        if (popcount(sys_mask) != 2 || popcount(proc_mask) != 2) ok = 0;
    }
    else { printf("winencpu: GetProcessAffinityMask failed (%lu)\n", GetLastError()); ok = 0; }

    GetLogicalProcessorInformation(NULL, &len);
    info = malloc(len);
    if (!GetLogicalProcessorInformation(info, &len))
    {
        printf("winencpu: GetLogicalProcessorInformation failed (%lu)\n", GetLastError());
        ok = 0;
    }
    else
    {
        ULONG_PTR all = 0;

        for (i = 0; i < len / sizeof(*info); i++)
            if (info[i].Relationship == RelationProcessorCore) all |= info[i].ProcessorMask;
        printf("winencpu: GetLogicalProcessorInformation: cores cover %d processors (%#lx)\n",
               popcount(all), (unsigned long)all);
        if (popcount(all) != 2) ok = 0;
    }

    len = 0;
    GetLogicalProcessorInformationEx(RelationAll, NULL, &len);
    ex = malloc(len);
    if (!GetLogicalProcessorInformationEx(RelationAll, ex, &len))
    {
        printf("winencpu: GetLogicalProcessorInformationEx failed (%lu)\n", GetLastError());
        ok = 0;
    }
    else
    {
        KAFFINITY cores = 0, groups = 0;
        DWORD active = 0;

        for (p = (char *)ex; p < (char *)ex + len; p += ((SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)p)->Size)
        {
            SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *e = (void *)p;

            if (e->Relationship == RelationProcessorCore)
                cores |= e->Processor.GroupMask[0].Mask;
            else if (e->Relationship == RelationGroup)
            {
                active += e->Group.GroupInfo[0].ActiveProcessorCount;
                groups |= e->Group.GroupInfo[0].ActiveProcessorMask;
            }
        }
        printf("winencpu: GetLogicalProcessorInformationEx: cores cover %d processors, group has %lu active (%#lx)\n",
               popcount(cores), active, (unsigned long)groups);
        if (popcount(cores) != 2 || active != 2 || popcount(groups) != 2) ok = 0;
    }

    {
        LONG (WINAPI *pNtQuerySystemInformation)(int, void *, ULONG, ULONG *) =
            (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation");
        /* SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION is 48 bytes */
        static char perf[48 * 256];
        ULONG ret_len = 0;
        LONG status = pNtQuerySystemInformation(8 /* SystemProcessorPerformanceInformation */,
                                                perf, sizeof(perf), &ret_len);

        printf("winencpu: SystemProcessorPerformanceInformation: status %#lx, %lu entries\n",
               status, ret_len / 48);
        if (status || ret_len / 48 != 2) ok = 0;
    }

    printf("winencpu: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
