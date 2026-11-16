/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Loaded into every Wine process of a run by helpers/occupy-exe-base
 * (DYLD_INSERT_LIBRARIES). If the file named by WMB_OCCUPY_FILE holds
 * "<hex address>:<hex size>", takes that range of the address space before
 * Wine maps anything, as a host mapping would. exe-base-taken writes the
 * file before starting a copy of itself, so the copy finds the base the
 * wineserver gives its exe already in use.
 */

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <stdio.h>
#include <stdlib.h>

__attribute__((constructor)) static void occupy(void)
{
    const char *path = getenv("WMB_OCCUPY_FILE");
    unsigned long long addr, size;
    mach_vm_address_t a;
    kern_return_t kr;
    FILE *f;
    int n;

    if (!path || !(f = fopen(path, "r"))) return;
    n = fscanf(f, "%llx:%llx", &addr, &size);
    fclose(f);
    if (n != 2) return;
    a = addr;
    kr = mach_vm_allocate(mach_task_self(), &a, size, VM_FLAGS_FIXED);
    fprintf(stderr, "occupy-exe-base: %#llx-%#llx: %s\n", addr, addr + size,
            kr == KERN_SUCCESS ? "taken" : mach_error_string(kr));
}
