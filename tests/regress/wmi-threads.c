/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Flavours: macfixes
 * Timeout: 60
 * Libs: -lole32 -loleaut32 -lwbemuuid
 * Guards: 0054 wbemprox-no-per-table-lock (10.0; Wine 10.5 has it)
 *
 * In Wine 10.0, wbemprox locks a WMI class's table from the query until the
 * caller releases what the query returned. A second thread querying the
 * same class meanwhile waits, and if the first is waiting for it the
 * program hangs; RtlpWaitForCriticalSection logs 60-second timeouts on
 * "dlls\wbemprox\builtin.c: table.cs". Big Fish games (White Wolf, Waverly
 * Academy) hung this way at start.
 *
 * One thread queries Win32_OperatingSystem and holds on to the result;
 * meanwhile four more threads each query the same class, read a row and
 * release it. Passes when they all finish within 10 s while the first still
 * holds its result.
 */

#define COBJMACROS
#include <windows.h>
#include <wbemcli.h>
#include <stdio.h>

static HANDLE held, done_event;
static LONG finished;

/* query Win32_OperatingSystem and read the first row's Caption */
static IEnumWbemClassObject *query(const char *who)
{
    IWbemLocator *locator = NULL;
    IWbemServices *services = NULL;
    IEnumWbemClassObject *result = NULL;
    IWbemClassObject *obj = NULL;
    BSTR ns = SysAllocString(L"ROOT\\CIMV2"), wql = SysAllocString(L"WQL"),
         text = SysAllocString(L"SELECT * FROM Win32_OperatingSystem");
    ULONG count = 0;
    VARIANT caption;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (void **)&locator);
    if (SUCCEEDED(hr)) hr = IWbemLocator_ConnectServer(locator, ns, NULL, NULL, NULL, 0, NULL, NULL, &services);
    if (SUCCEEDED(hr))
        hr = IWbemServices_ExecQuery(services, wql, text, WBEM_FLAG_FORWARD_ONLY, NULL, &result);
    if (SUCCEEDED(hr)) hr = IEnumWbemClassObject_Next(result, 10000, 1, &obj, &count);
    if (SUCCEEDED(hr) && count)
    {
        VariantInit(&caption);
        IWbemClassObject_Get(obj, L"Caption", 0, &caption, NULL, NULL);
        VariantClear(&caption);
        IWbemClassObject_Release(obj);
    }
    else printf("wmi-threads: %s: the query failed (%#lx)\n", who, hr);
    if (services) IWbemServices_Release(services);
    if (locator) IWbemLocator_Release(locator);
    SysFreeString(ns);
    SysFreeString(wql);
    SysFreeString(text);
    return result;
}

static DWORD WINAPI holder(void *arg)
{
    IEnumWbemClassObject *result;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    result = query("the first thread");
    SetEvent(held);
    WaitForSingleObject(done_event, 20000);  /* hold it until the others are done */
    if (result) IEnumWbemClassObject_Release(result);
    CoUninitialize();
    return 0;
}

static DWORD WINAPI querier(void *arg)
{
    IEnumWbemClassObject *result;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if ((result = query("another thread"))) IEnumWbemClassObject_Release(result);
    CoUninitialize();
    if (InterlockedIncrement(&finished) == 4) SetEvent(done_event);
    return 0;
}

int main(void)
{
    HANDLE threads[5];
    DWORD start, r;
    int i;

    held = CreateEventA(NULL, TRUE, FALSE, NULL);
    done_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    threads[0] = CreateThread(NULL, 0, holder, NULL, 0, NULL);
    WaitForSingleObject(held, 30000);

    start = GetTickCount();
    for (i = 1; i < 5; i++) threads[i] = CreateThread(NULL, 0, querier, NULL, 0, NULL);
    r = WaitForSingleObject(done_event, 10000);
    printf("wmi-threads: %ld of 4 queries finished in %lu ms while the first thread held its result\n",
           finished, GetTickCount() - start);
    SetEvent(done_event);
    if (r != WAIT_OBJECT_0)
    {
        printf("wmi-threads: FAILED\n");
        ExitProcess(1);  /* the stuck threads would keep it running */
    }
    WaitForMultipleObjects(5, threads, TRUE, 10000);
    printf("wmi-threads: ok\n");
    return 0;
}
