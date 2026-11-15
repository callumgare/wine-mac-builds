/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * audio.exe: can this Wine build play sound? Opens the default output
 * device with waveOut (which goes through mmdevapi to winecoreaudio, so
 * through CoreAudio), plays a quarter of a second of silence and waits for
 * the device to say it finished. Silence, so a test run makes no noise.
 *
 * Exits 0 if it played; 1 if opening or playing failed; 2 if Wine sees no
 * output device at all (the caller decides whether the Mac has one: a CI
 * VM may not).
 *
 * Built by build/smoke.sh with the toolchain's llvm-mingw (plan.md
 * section 4).
 */

#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static short samples[44100 / 4 * 2]; /* 0.25 s, 16-bit stereo, silent */
    WAVEFORMATEX fmt = { 0 };
    WAVEHDR hdr = { 0 };
    HWAVEOUT wo;
    MMRESULT res;
    UINT devs = waveOutGetNumDevs();
    DWORD start;

    printf("audio: %u output device(s)\n", devs);
    if (!devs)
        return 2;

    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = 44100;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = fmt.nChannels * fmt.wBitsPerSample / 8;
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
    if ((res = waveOutOpen(&wo, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL)))
    {
        printf("audio: FAILED: waveOutOpen returned %u\n", res);
        return 1;
    }
    memset(samples, 0, sizeof(samples));
    hdr.lpData = (LPSTR)samples;
    hdr.dwBufferLength = sizeof(samples);
    if ((res = waveOutPrepareHeader(wo, &hdr, sizeof(hdr))) ||
        (res = waveOutWrite(wo, &hdr, sizeof(hdr))))
    {
        printf("audio: FAILED: waveOutWrite returned %u\n", res);
        waveOutClose(wo);
        return 1;
    }
    start = GetTickCount();
    while (!(hdr.dwFlags & WHDR_DONE) && GetTickCount() - start < 5000)
        Sleep(20);
    printf("audio: buffer %s after %lu ms\n", (hdr.dwFlags & WHDR_DONE) ? "played" : "NOT played",
           (unsigned long)(GetTickCount() - start));
    waveOutReset(wo);
    waveOutUnprepareHeader(wo, &hdr, sizeof(hdr));
    waveOutClose(wo);
    if (!(hdr.dwFlags & WHDR_DONE))
    {
        printf("audio: FAILED: the device never finished the buffer\n");
        return 1;
    }
    printf("audio: ok\n");
    return 0;
}
