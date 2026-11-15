/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * media.exe: does this Wine build's Media Foundation have working
 * decoders? For H.264 video and AAC audio, finds the registered decoder
 * transforms and creates the first of each. Creating one makes
 * winegstreamer check that GStreamer (or, in Wine 11, FFmpeg through
 * winedmo) can really decode the format, so a media stack that can't load
 * its libraries fails here, although the build boots fine
 * (dappermint/winecx-gptk shipped one, broken by a libiconv mismatch).
 *
 * Exits 0 if both decoders were created, 1 otherwise.
 *
 * Built by build/smoke.sh with the toolchain's llvm-mingw, linked with
 * -lmfplat -lmfuuid -lole32 (plan.md section 4).
 */

#define COBJMACROS
#include <windows.h>
#include <mfapi.h>
#include <mftransform.h>
#include <mferror.h>
#include <stdio.h>

static int try_decoder(const char *what, GUID category, GUID major, GUID subtype)
{
    MFT_REGISTER_TYPE_INFO in = { major, subtype };
    IMFActivate **acts = NULL;
    IMFTransform *mft = NULL;
    UINT32 count = 0, i;
    HRESULT hr;
    int ok = 0;

    hr = MFTEnumEx(category, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT | MFT_ENUM_FLAG_HARDWARE |
                   MFT_ENUM_FLAG_LOCALMFT | MFT_ENUM_FLAG_SORTANDFILTER, &in, NULL, &acts, &count);
    printf("media: %s: %u decoder(s) registered (hr 0x%08lx)\n", what, count, (unsigned long)hr);
    if (SUCCEEDED(hr) && count)
    {
        WCHAR *name = NULL;
        UINT32 len;
        if (SUCCEEDED(IMFActivate_GetAllocatedString(acts[0], &MFT_FRIENDLY_NAME_Attribute, &name, &len)))
        {
            printf("media: %s: first is %ls\n", what, name);
            CoTaskMemFree(name);
        }
        hr = IMFActivate_ActivateObject(acts[0], &IID_IMFTransform, (void **)&mft);
        printf("media: %s: creating it %s (hr 0x%08lx)\n", what, SUCCEEDED(hr) ? "worked" : "FAILED",
               (unsigned long)hr);
        if (SUCCEEDED(hr))
        {
            ok = 1;
            IMFTransform_Release(mft);
            IMFActivate_ShutdownObject(acts[0]);
        }
    }
    for (i = 0; i < count; i++)
        IMFActivate_Release(acts[i]);
    CoTaskMemFree(acts);
    return ok;
}

int main(void)
{
    int ok;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr = MFStartup(MF_VERSION, MFSTARTUP_FULL)))
    {
        printf("media: FAILED: MFStartup returned 0x%08lx\n", (unsigned long)hr);
        return 1;
    }
    ok = try_decoder("H.264", MFT_CATEGORY_VIDEO_DECODER, MFMediaType_Video, MFVideoFormat_H264);
    ok &= try_decoder("AAC", MFT_CATEGORY_AUDIO_DECODER, MFMediaType_Audio, MFAudioFormat_AAC);
    MFShutdown();
    CoUninitialize();
    printf(ok ? "media: ok\n" : "media: FAILED: a decoder couldn't be created\n");
    return ok ? 0 : 1;
}
