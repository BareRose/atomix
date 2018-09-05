/*
atomix.h example for command line usage like "test.exe mu.ogg so.ogg" performing benchmarks and demos

To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring
rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software.
If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

/*
Compile using "gcc -O2 test.c -o test.exe" or equivalent, then run from command line to use.
Most modern compilers will have SSE enabled by default, if not you need a flag like "-msse".
If you find an error in this test or discover a possible improvement, please open an issue.
*/

//includes
#define ATOMIX_STATIC
#include "atomix.h"
#define STB_VORBIS_NO_INTEGER_CONVERSION
#define STB_VORBIS_IMPLEMENTATION
#define STB_VORBIS_STATIC
#include "libs/stb_vorbis.h"
#define MINI_AL_IMPLEMENTATION
#include "libs/mini_al.h"
#include <stdlib.h>
#include <stdio.h>

//send frames callback
mal_uint32 sendFramesCallback (mal_device* dev, mal_uint32 fnum, void* buff) {
    return atomixMixerMix(dev->pUserData, buff, fnum);
}

//benchmarking
#ifdef WIN32
    #include <windows.h>
    double getTime () {
        LARGE_INTEGER t, f;
        QueryPerformanceCounter(&t);
        QueryPerformanceFrequency(&f);
        return (double)t.QuadPart/(double)f.QuadPart;
    }
    void psleep (double s) {
        Sleep(s*1000.0);
    }
#else
    #include <sys/time.h>
    double getTime () {
        struct timeval t;
        gettimeofday(&t, NULL);
        return t.tv_sec + t.tv_usec*1e-6;
    }
    void psleep (double s) {
        struct timespec t = {s, s*1e-6};
        nanosleep(&t, NULL);
    }
#endif

//main function
int main (int argc, char *argv[]) {
    //perpare variables
    mal_device dev;
    float bench_buff[1024];
    void* fmus; void* fsnd;
    mal_uint64 fmus_size, fsnd_size;
    mal_decoder_config fmus_cfg, fsnd_cfg;
    fmus_cfg = fsnd_cfg = mal_decoder_config_init(mal_format_f32, 0, MAL_SAMPLE_RATE_48000);
    //initialize rand
    srand(getTime()*65536.0);
    //check arguments
    if (argc < 3) printf("Missing argument!\n");
    else if (mal_decode_file(argv[1], &fmus_cfg, &fmus_size, &fmus) != MAL_SUCCESS)
        printf("Music could not be loaded!\n");
    else if (mal_decode_file(argv[2], &fsnd_cfg, &fsnd_size, &fsnd) != MAL_SUCCESS)
        printf("Sound could not be loaded!\n");
    else {
        //transfer audio data into atomix sounds and free temporary buffers
        struct atomix_sound* mus; struct atomix_sound* snd;
        mus = atomixSoundNew(fmus_cfg.channels, fmus, fmus_size);
        snd = atomixSoundNew(fsnd_cfg.channels, fsnd, fsnd_size);
        mal_free(fmus); mal_free(fsnd);
        //create atomix mixer with volume of 0.5
        struct atomix_mixer* mix = atomixMixerNew(0.5f, 0);
        //begin benchmarking
        printf("<<BENCHMARK BEGIN>>\n");
        //mix 512 at a time 512 times with 256 sounds
        for (int i = 0; i < 256; i++) atomixMixerPlay(mix, mus, ATOMIX_LOOP, 1.0f, 0.0f);
        double start = getTime();
        for (int i = 0; i < 512; i++) atomixMixerMix(mix, bench_buff, 512);
        double end = getTime();
        atomixMixerStopAll(mix); //mark all layers for clearing
        atomixMixerMix(mix, bench_buff, 512); //make sure layers are actually cleared
        printf("256: %.0ff/s <- %.0ff/s (%.3fMiB/s)\n", 262144.0/(end-start), 16777216.0/(end-start), 2.0/(end-start));
        //mix 512 at a time 512 times with single sound
        atomixMixerPlay(mix, mus, ATOMIX_LOOP, 1.0f, 0.0f);
        start = getTime();
        for (int i = 0; i < 512; i++) atomixMixerMix(mix, bench_buff, 512);
        end = getTime();
        atomixMixerStopAll(mix); //mark all layers for clearing
        atomixMixerMix(mix, bench_buff, 512); //make sure layers are actually cleared
        printf("One: %.0ff/s <- %.0ff/s (%.3fMiB/s)\n", 262144.0/(end-start), 262144.0/(end-start), 2.0/(end-start));
        //benchmarking done
        printf("<<BENCHMARK END>>\n");
        //create mini_al playback device
        mal_device_config cfg = mal_device_config_init_playback(mal_format_f32, 2, MAL_SAMPLE_RATE_48000, sendFramesCallback);
        if (mal_device_init(NULL, mal_device_type_playback, NULL, &cfg, mix, &dev) != MAL_SUCCESS) {
            //failed to initialize device
            printf("Failed to initialize device!\n");
            //return
            return 1;
        }
        //start playback device
        mal_device_start(&dev);
        //set fade time to quarter of a second
        atomixMixerFade(mix, 12000);
        //begin demo of simple looping music and sound halting
        printf("<<DEMO BEGIN>>\n");
        atomixMixerPlay(mix, mus, ATOMIX_LOOP, 0.25f, 0.0f);
        uint32_t sid = atomixMixerPlay(mix, snd, ATOMIX_HALT, 1.0f, 0.0f);
        //loop and play sound in various ways every so often
        for (int i = 0; i < 8; i++) {
            //sleep for 0.5 second
            psleep(0.5);
            //resume sound with random panning
            float r = (float)rand()/(float)RAND_MAX;
            atomixMixerSetGainPan(mix, sid, 1.0f, 2.0f*(r - 0.5f));
            atomixMixerSetState(mix, sid, ATOMIX_LOOP);
            //sleep for 0.5 seconds
            psleep(0.5);
            //halt sound whatever its position
            atomixMixerSetState(mix, sid, ATOMIX_HALT);
        }
        //end demo by stopping all sounds
        atomixMixerStopAll(mix);
        printf("<<DEMO END>>\n");
        //sleep for 0.25 seconds
        psleep(0.25);
        //uninit playback device
        mal_device_uninit(&dev);
        //free mixer and sounds
        free(mix); free(mus); free(snd);
    }
    //return
    return 0;
}