/*
atomix.h - Portable, single-file, wait-free atomic sound mixing library utilizing SSE-accelerated mixing

To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring
rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software.
If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

/*
atomix supports the following three configurations:
#define ATOMIX_EXTERN
    Default, should be used when using atomix in multiple compilation units within the same project.
#define ATOMIX_IMPLEMENTATION
    Must be defined in exactly one source file within a project for atomix to be found by the linker.
#define ATOMIX_STATIC
    Defines all atomix functions as static, useful if atomix is only used in a single compilation unit.

atomix supports the following additional options:
#define ATOMIX_NO_CLIP
    Disables internal clipping, useful if you are using a backend that already does clipping of its own.
#define ATOMIX_NO_SSE
    Disables all SSE optimizations, which makes atomix mix about 4 times slower but also use less memory.
#define ATOMIX_LBITS
    Determines the number of layers as a power of 2. For example the default value of 8 means 256 layers.
#define ATOMIX_ZALLOC(S)
    Overrides the zalloc function used by atomix with your own. This is calloc but with just 1 argument.

atomix threads:
    Atomix is built around having one thread occasionally calling atomixMixerMix (usually in a callback)
    and one other thread (usually the main thread) calling the other functions to play/stop/etc sounds.
    Anything more than this will likely break thread safety and result in various problems and/or bugs.

atomix frames:
    A frame refers to a number of samples equal to the number of channels, so usually two floats as one
    sample is a single float and atomix works primarily in stereo. Calling atomixSoundNew with a channel
    count of one is the one exception where a frame is a single sample as there is only one channel.

atomix fading:
    Fading out happens automatically when a playing sound is stopped (changed to ATOMIX_STOP or ATOMIX_HALT).
    Fading in happens when a sound is resumed (changed to ATOMIX_PLAY or ATOMIX_LOOP) after having been halted.
    Fading out will not happen if the sound is close to its end, in which case it will simply play out instead.
    A sound started in a halted state will start fully faded out, resulting in a fade in when it is unhalted.

atomix limits (SSE):
    The atomixMixerMix function uses a buffer on the stack which scales with the number of frames requested,
    therefore requesting an excessively high number of frames at once will likely result in a stack overflow.
    All frame numbers passed to atomix functions such as cursor positions, start and end frames, fade timers,
    and sound lengths are rounded to multiples of 4 in some way for reasons relating to internal alignment.

atomix limits (non-SSE):
    Without SSE alignment requirements the atomixMixerMix no longer has to use a buffer on the stack, which
    removes the risk of stack overflow when mixing lots of frames at once. Memory usage is lower in general.
    Frame numbers passed to atomix functions are still rounded to multiples of 4 to keep the API consistent.
    Internally things are slightly different, as frames are now processed one-by-one instead of 4 at a time.
*/

//header section
#ifndef ATOMIX_H
#define ATOMIX_H

//process configuration
#ifdef ATOMIX_STATIC
    #define ATOMIX_IMPLEMENTATION
    #define ATMXDEF static
#else //ATOMIX_EXTERN
    #define ATMXDEF extern
#endif

//constants
#define ATOMIX_STOP 1
#define ATOMIX_HALT 2
#define ATOMIX_PLAY 3
#define ATOMIX_LOOP 4

//includes
#include <stdint.h> //integer types
struct atomix_mixer; //forward declaration
struct atomix_sound; //forward declaration

//function declarations
ATMXDEF struct atomix_sound* atomixSoundNew(uint8_t, float*, int32_t);
    //creates a new atomix sound with given number of channels and data
    //length of data is in frames and rounded to multiple of 4 for alignment
    //given data is copied, so the buffer can safely be freed after return
    //returns a pointer to the new atomix sound or NULL on failure
ATMXDEF int32_t atomixSoundLength(struct atomix_sound*);
    //returns the length of given sound in frames, always multiple of 4
ATMXDEF struct atomix_mixer* atomixMixerNew(float, int32_t);
    //returns a new atomix mixer with given volume and fade or NULL on failure to allocate
ATMXDEF uint32_t atomixMixerMix(struct atomix_mixer*, float*, uint32_t);
    //uses given atomix mixer to output exactly the requested number of frames to given buffer
    //returns the number of frames actually written to the buffer, buffer must not be NULL
ATMXDEF uint32_t atomixMixerPlay(struct atomix_mixer*, struct atomix_sound*, uint8_t, float, float);
    //uses given atomix mixer to play given atomix sound with given initial state, gain, and pan
    //returns a sound handle used to reference the sound at a later point, or 0 on failure
ATMXDEF uint32_t atomixMixerPlayAdv(struct atomix_mixer*, struct atomix_sound*, uint8_t, float, float, int32_t, int32_t, int32_t);
    //variant of atomixPlay that sets the start and end frames in the sound, positions are truncated to multiple of 4
    //a negative start value can be used to play a sound with a delay, a high end value can be used to loop a few times
    //if in the ATOMIX_LOOP state, looping will include these start/end positions, allowing for partial sounds to loop
    //returns a sound handle used to reference the sound at a later point, or 0 on failure
ATMXDEF int atomixMixerSetGainPan(struct atomix_mixer*, uint32_t, float, float);
    //sets the gain and pan for the sound with given handle in given mixer
    //gain may be any float including negative, pan is clamped internally
    //returns 0 on success, non-zero if the handle is invalid
ATMXDEF int atomixMixerSetCursor(struct atomix_mixer*, uint32_t, int32_t);
    //sets the cursor for the sound with given handle in given mixer
    //given cursor value is clamped and truncated to multiple of 4
    //returns 0 on success, non-zero if the handle is invalid
ATMXDEF int atomixMixerSetState(struct atomix_mixer*, uint32_t, uint8_t);
    //sets the state for the sound with given handle in given mixer
    //given state must be one of the ATOMIX_XXX define constants
    //returns 0 on success, non-zero if the handle is invalid
ATMXDEF void atomixMixerVolume(struct atomix_mixer*, float);
    //sets the global volume for given atomix mixer, may be any float including negative
ATMXDEF void atomixMixerFade(struct atomix_mixer*, int32_t);
    //sets the global default fade value applied to all new sounds added after this command
ATMXDEF void atomixMixerStopAll(struct atomix_mixer*);
    //stops all sounds in given mixer, invalidating any existing sound handles in that mixer
ATMXDEF void atomixMixerHaltAll(struct atomix_mixer*);
    //halts all sounds currently playing in given mixer, allowing them to be resumed later
ATMXDEF void atomixMixerPlayAll(struct atomix_mixer*);
    //resumes all halted sounds in given mixer, no effect on looping or stopped sounds

#endif //ATOMIX_H

//implementation section
#ifdef ATOMIX_IMPLEMENTATION
#undef ATOMIX_IMPLEMENTATION

//constants
#ifndef ATOMIX_LBITS
    #define ATOMIX_LBITS 8
#endif
#define ATMX_LAYERS (1 << ATOMIX_LBITS)
#define ATMX_LMASK (ATMX_LAYERS - 1)

//macros
#ifndef ATOMIX_ZALLOC
    #include <stdlib.h> //calloc
    #define ATOMIX_ZALLOC(S) calloc(1, S)
#endif
#define ATMX_STORE(A, C) atomic_store_explicit(A, C, memory_order_release)
#define ATMX_LOAD(A) atomic_load_explicit(A, memory_order_acquire)
#define ATMX_CSWAP(A, E, C) atomic_compare_exchange_strong_explicit(A, E, C, memory_order_acq_rel, memory_order_acquire)

//includes
#ifndef ATOMIX_NO_SSE
    #include <xmmintrin.h> //SSE intrinsics
#endif
#include <stdatomic.h> //atomics
#include <string.h> //memcpy

//structs
struct atomix_sound {
    uint8_t cha; //channels
    int32_t len; //data length
    #ifndef ATOMIX_NO_SSE
        __m128* data; //aligned data
    #else
        float data[]; //float data
    #endif
};
struct atmx_f2 {
    float l, r; //left/right floats
};
struct atmx_layer {
    uint32_t id; //playing id
    _Atomic uint8_t flag; //state
    _Atomic int32_t cursor; //cursor
    _Atomic struct atmx_f2 gain; //gain
    struct atomix_sound* snd; //sound data
    int32_t start, end; //start and end
    int32_t fade, fmax; //fading
};
struct atomix_mixer {
    uint32_t nid; //next id
    _Atomic float volume; //global volume
    struct atmx_layer lays[ATMX_LAYERS]; //layers
    int32_t fade; //global default fade value
    #ifndef ATOMIX_NO_SSE
        uint32_t rem; //remaining frames
        float data[6]; //old frames
    #endif
};

//function declarations
#ifndef ATOMIX_NO_SSE
    static void atmxMixLayer(struct atmx_layer*, __m128, __m128*, uint32_t);
    static int32_t atmxMixFadeMono(struct atmx_layer*, int32_t, __m128, __m128*, uint32_t);
    static int32_t atmxMixFadeStereo(struct atmx_layer*, int32_t, __m128, __m128*, uint32_t);
    static int32_t atmxMixPlayMono(struct atmx_layer*, int, int32_t, __m128, __m128*, uint32_t);
    static int32_t atmxMixPlayStereo(struct atmx_layer*, int, int32_t, __m128, __m128*, uint32_t);
#else
    static void atmxMixLayer(struct atmx_layer*, float, float*, uint32_t);
    static int32_t atmxMixFadeMono(struct atmx_layer*, int32_t, struct atmx_f2, float*, uint32_t);
    static int32_t atmxMixFadeStereo(struct atmx_layer*, int32_t, struct atmx_f2, float*, uint32_t);
    static int32_t atmxMixPlayMono(struct atmx_layer*, int, int32_t, struct atmx_f2, float*, uint32_t);
    static int32_t atmxMixPlayStereo(struct atmx_layer*, int, int32_t, struct atmx_f2, float*, uint32_t);
#endif
static struct atmx_f2 atmxGainf2(float, float);

//public functions
ATMXDEF struct atomix_sound* atomixSoundNew (uint8_t cha, float* data, int32_t len) {
    //validate arguments first and return NULL if invalid
    if ((cha < 1)||(cha > 2)||(!data)||(len < 1)) return NULL;
    //round length to next multiple of 4
    int32_t rlen = (len + 3) & ~0x03;
    //allocate sound struct and space for data
    #ifndef ATOMIX_NO_SSE
        struct atomix_sound* snd = ATOMIX_ZALLOC(sizeof(struct atomix_sound) + rlen*cha*sizeof(float) + 15);
    #else
        struct atomix_sound* snd = ATOMIX_ZALLOC(sizeof(struct atomix_sound) + rlen*cha*sizeof(float));
    #endif
    //return if zalloc failed
    if (!snd) return NULL;
    //fill in channel and length
    snd->cha = cha; snd->len = rlen;
    //align data pointer in allocated space if SSE
    #ifndef ATOMIX_NO_SSE
        snd->data = (void*)(((uintptr_t)(void*)&snd[1] + 15) & ~15);
    #endif
    //copy sound data into now aligned buffer
    memcpy(snd->data, data, len*cha*sizeof(float));
    //return
    return snd;
}
ATMXDEF int32_t atomixSoundLength (struct atomix_sound* snd) {
    //return length, always multiple of 4
    return snd->len;
}
ATMXDEF struct atomix_mixer* atomixMixerNew (float vol, int32_t fade) {
    //allocate space for the mixer filled with zero
    struct atomix_mixer* mix = ATOMIX_ZALLOC(sizeof(struct atomix_mixer));
    //return if zalloc failed
    if (!mix) return NULL;
    //atomically set the volume
    ATMX_STORE(&mix->volume, vol);
    //set fade value
    mix->fade = (fade < 0) ? 0 : fade & ~3;
    //return
    return mix;
}
ATMXDEF uint32_t atomixMixerMix (struct atomix_mixer* mix, float* buff, uint32_t fnum) {
    //the mixing function differs greatly depending on whether SSE is enabled or not
    #ifndef ATOMIX_NO_SSE
        //output remaining frames in buffer before mixing new ones
        uint32_t rnum = fnum;
        //only do this if there are old frames
        if (mix->rem)
            if (rnum > mix->rem) {
                //rnum bigger than remaining frames (usual case)
                memcpy(buff, mix->data, mix->rem*2*sizeof(float));
                rnum -= mix->rem; buff += mix->rem*2; mix->rem = 0;
            } else {
                //rnum smaller equal remaining frames (rare case)
                memcpy(buff, mix->data, rnum*2*sizeof(float)); mix->rem -= rnum;
                //move back remaining old frames if any
                if (mix->rem) memmove(mix->data, mix->data + rnum*2, (3 - rnum)*2*sizeof(float));
                //return
                return fnum;
            }
        //asize in __m128 (__m128 = 2 frames) and multiple of 2
        uint32_t asize = ((rnum + 3) & ~3) >> 1;
        //dynamically sized aligned buffer
        __m128 align[asize];
        //clear the aligned buffer using SSE assignment
        for (uint32_t i = 0; i < asize; i++) align[i] = _mm_setzero_ps();
        //begin actual mixing, caching the volume first
        __m128 vol = _mm_set_ps1(ATMX_LOAD(&mix->volume));
        for (int i = 0; i < ATMX_LAYERS; i++) atmxMixLayer(&mix->lays[i], vol, align, asize);
        //perform clipping using SSE min and max (unless disabled)
        #ifndef ATOMIX_NO_CLIP
            __m128 neg1 = _mm_set_ps1(-1.0f), pos1 = _mm_set_ps1(1.0f);
            for (uint32_t i = 0; i < asize; i++) align[i] = _mm_min_ps(_mm_max_ps(align[i], neg1), pos1);
        #endif
        //copy rnum frames, leaving possible remainder
        memcpy(buff, align, rnum*2*sizeof(float));
        //determine remaining number of frames
        mix->rem = asize*2 - rnum;
        //copy remaining frames to buffer inside the mixer struct
        if (mix->rem) memcpy(mix->data, (float*)align + rnum*2, mix->rem);
    #else
        //clear the output buffer using memset
        memset(buff, 0, fnum*2*sizeof(float));
        //begin actual mixing, caching the volume first
        float vol = ATMX_LOAD(&mix->volume);
        for (int i = 0; i < ATMX_LAYERS; i++) atmxMixLayer(&mix->lays[i], vol, buff, fnum);
        //perform clipping using simple ternary operators (unless disabled)
        #ifndef ATOMIX_NO_CLIP
            for (uint32_t i = 0; i < fnum*2; i++) buff[i] = (buff[i] < -1.0f) ? -1.0f : (buff[i] > 1.0f) ? 1.0f : buff[i];
        #endif
    #endif
    //return
    return fnum;
}
ATMXDEF uint32_t atomixMixerPlay (struct atomix_mixer* mix, struct atomix_sound* snd, uint8_t flag, float gain, float pan) {
    //play with start and end equal to start and end of the sound itself
    return atomixMixerPlayAdv(mix, snd, flag, gain, pan, 0, snd->len, mix->fade);
}
ATMXDEF uint32_t atomixMixerPlayAdv (struct atomix_mixer* mix, struct atomix_sound* snd, uint8_t flag, float gain, float pan, int32_t start, int32_t end, int32_t fade) {
    //return failure if given flag invalid
    if ((flag < 1)||(flag > 4)) return 0;
    //return failure if start or end invalid
    if ((end - start < 4)||(end < 4)) return 0;
    //make ATMX_LAYERS attempts to find layer
    for (int i = 0; i < ATMX_LAYERS; i++) {
        //get layer for next sound handle id
        uint32_t id; struct atmx_layer* lay = &mix->lays[(id = mix->nid++) & ATMX_LMASK];
        //check if corresponding layer is free
        if (ATMX_LOAD(&lay->flag) == 0) {
            //skip 0 as it is special
            if (!id) id = ATMX_LAYERS;
            //fill in non-atomic layer data along with truncating start and end
            lay->id = id; lay->snd = snd;
            lay->start = start & ~3; lay->end = end & ~3;
            lay->fmax = (fade < 0) ? 0 : fade & ~3;
            //set initial fade state based on flag
            lay->fade = (flag < 3) ? 0 : lay->fmax;
            //convert gain and pan to left and right gain and store it atomically
            ATMX_STORE(&lay->gain, atmxGainf2(gain, pan));
            //atomically set cursor to start position based on given argument
            ATMX_STORE(&lay->cursor, lay->start);
            //store flag last, releasing the layer to the mixer thread
            ATMX_STORE(&lay->flag, flag);
            //return success
            return id;
        }
    }
    //return failure
    return 0;
}
ATMXDEF int atomixMixerSetGainPan (struct atomix_mixer* mix, uint32_t id, float gain, float pan) {
    //get layer based on the lowest bits of id
    struct atmx_layer* lay = &mix->lays[id & ATMX_LMASK];
    //check id and state flag to make sure the id is valid
    if ((id == lay->id)&&(ATMX_LOAD(&lay->flag) > 1)) {
        //convert gain and pan to left and right gain and store it atomically
        ATMX_STORE(&lay->gain, atmxGainf2(gain, pan));
        //return success
        return 1;
    }
    //return failure
    return 0;
}
ATMXDEF int atomixMixerSetCursor (struct atomix_mixer* mix, uint32_t id, int32_t cursor) {
    //get layer based on the lowest bits of id
    struct atmx_layer* lay = &mix->lays[id & ATMX_LMASK];
    //check id and state flag to make sure the id is valid
    if ((id == lay->id)&&(ATMX_LOAD(&lay->flag) > 1)) {
        //clamp cursor and truncate to multiple of 4 before storing
        ATMX_STORE(&lay->cursor, (cursor < lay->start) ? lay->start : (cursor > lay->end) ? lay->end : cursor & ~3);
        //return success
        return 1;
    }
    //return failure
    return 0;
}
ATMXDEF int atomixMixerSetState (struct atomix_mixer* mix, uint32_t id, uint8_t flag) {
    //return failure if given flag invalid
    if ((flag < 1)||(flag > 4)) return 0;
    //get layer based on the lowest bits of id
    struct atmx_layer* lay = &mix->lays[id & ATMX_LMASK]; uint8_t prev;
    //check id and state flag to make sure the id is valid
    if ((id == lay->id)&&((prev = ATMX_LOAD(&lay->flag)) > 1)) {
        //return success if already in desired state
        if (prev == flag) return 1;
        //swap if flag has not changed and return if successful
        if (ATMX_CSWAP(&lay->flag, &prev, flag)) return 1;
    }
    //return failure
    return 0;
}
ATMXDEF void atomixMixerVolume (struct atomix_mixer* mix, float vol) {
    //simple atomic store of the volume
    ATMX_STORE(&mix->volume, vol);
}
ATMXDEF void atomixMixerFade (struct atomix_mixer* mix, int32_t fade) {
    //simple assignment of the fade value
    mix->fade = (fade < 0) ? 0 : fade & ~3;
}
ATMXDEF void atomixMixerStopAll (struct atomix_mixer* mix) {
    //go through all active layers and set their states to the stop state
    for (int i = 0; i < ATMX_LAYERS; i++) {
        //pointer to this layer for cleaner code
        struct atmx_layer* lay = &mix->lays[i];
        //check if active and set to stop if true
        if (ATMX_LOAD(&lay->flag) > 1) ATMX_STORE(&lay->flag, ATOMIX_STOP);
    }
}
ATMXDEF void atomixMixerHaltAll (struct atomix_mixer* mix) {
    //go through all playing layers and set their states to halt
    for (int i = 0; i < ATMX_LAYERS; i++) {
        //pointer to this layer for cleaner code
        struct atmx_layer* lay = &mix->lays[i]; uint8_t flag;
        //check if playing or looping and try to swap
        if ((flag = ATMX_LOAD(&lay->flag)) > 2) ATMX_CSWAP(&lay->flag, &flag, ATOMIX_HALT);
    }
}
ATMXDEF void atomixMixerPlayAll (struct atomix_mixer* mix) {
    //go through all halted layers and set their states to play
    for (int i = 0; i < ATMX_LAYERS; i++) {
        //need to reset each time
        uint8_t flag = ATOMIX_HALT;
        //swap the flag to play if it is on halt
        ATMX_CSWAP(&mix->lays[i].flag, &flag, ATOMIX_PLAY);
    }
}

//internal functions
#ifndef ATOMIX_NO_SSE
static void atmxMixLayer (struct atmx_layer* lay, __m128 vol, __m128* align, uint32_t asize) {
    //load flag value atomically first
    uint8_t flag = ATMX_LOAD(&lay->flag);
    //return if flag cleared
    if (flag == 0) return;
    //atomically load cursor
    int32_t cur = ATMX_LOAD(&lay->cursor);
    //atomically load left and right gain
    struct atmx_f2 g = ATMX_LOAD(&lay->gain);
    __m128 gmul = _mm_mul_ps(_mm_setr_ps(g.l, g.r, g.l, g.r), vol);
    //action based on flag
    if (flag < 3) {
        //ATOMIX_STOP or ATOMIX_HALT, fade out if not faded or at end
        if ((lay->fade > 0)&&(cur < lay->end)) 
            if (lay->snd->cha == 1)
                cur = atmxMixFadeMono(lay, cur, gmul, align, asize);
            else
                cur = atmxMixFadeStereo(lay, cur, gmul, align, asize);
        //clear flag if ATOMIX_STOP and fully faded or at end
        if ((flag == ATOMIX_STOP)&&((lay->fade == 0)||(cur == lay->end))) ATMX_STORE(&lay->flag, 0);
    } else {
        //ATOMIX_PLAY or ATOMIX_LOOP, play including fade in
        if (lay->snd->cha == 1)
            cur = atmxMixPlayMono(lay, (flag == ATOMIX_LOOP), cur, gmul, align, asize);
        else
            cur = atmxMixPlayStereo(lay, (flag == ATOMIX_LOOP), cur, gmul, align, asize);
        //clear flag if ATOMIX_PLAY and the cursor has reached the end
        if ((flag == ATOMIX_PLAY)&&(cur == lay->end)) ATMX_CSWAP(&lay->flag, &flag, 0);
    }
}
static int32_t atmxMixFadeMono (struct atmx_layer* lay, int32_t cur, __m128 gmul, __m128* align, uint32_t asize) {
    //cache cursor
    int32_t old = cur;
    //check if enough samples left for fade out
    if (lay->fade < lay->end - cur) {
        //perform fade out
        for (uint32_t i = 0; i < asize; i += 2) {
            //quit if fully faded out
            if (lay->fade == 0) break;
            //mix if cursor within sound
            if (cur >= 0) {
                //get faded volume multiplier
                __m128 fmul = _mm_mul_ps(_mm_set_ps1((float)lay->fade/(float)lay->fmax), gmul);
                //load 4 samples from data (this is 4 frames)
                __m128 sam = lay->snd->data[(cur % lay->snd->len) >> 2];
                //mix low samples obtained with unpacklo
                align[i] = _mm_add_ps(align[i], _mm_mul_ps(_mm_unpacklo_ps(sam, sam), fmul));
                //mix high samples obtained with unpackhi
                align[i+1] = _mm_add_ps(align[i+1], _mm_mul_ps(_mm_unpackhi_ps(sam, sam), fmul));
            }
            //advance cursor and fade
            lay->fade -= 4; cur += 4;
        }
    } else {
        //continue playback to end without fade out
        for (uint32_t i = 0; i < asize; i += 2) {
            //quit if cursor at end
            if (cur == lay->end) break;
            //mix if cursor within sound
            if (cur >= 0) {
                //load 4 samples from data (this is 4 frames)
                __m128 sam = lay->snd->data[(cur % lay->snd->len) >> 2];
                //mix low samples obtained with unpacklo
                align[i] = _mm_add_ps(align[i], _mm_mul_ps(_mm_unpacklo_ps(sam, sam), gmul));
                //mix high samples obtained with unpackhi
                align[i+1] = _mm_add_ps(align[i+1], _mm_mul_ps(_mm_unpackhi_ps(sam, sam), gmul));
            }
            //advance cursor
            cur += 4;
        }
    }
    //swap back cursor if unchanged
    if (!ATMX_CSWAP(&lay->cursor, &old, cur)) cur = old;
    //return new cursor
    return cur;
}
static int32_t atmxMixFadeStereo (struct atmx_layer* lay, int32_t cur, __m128 gmul, __m128* align, uint32_t asize) {
    //cache cursor
    int32_t old = cur;
    //check if enough samples left for fade out
    if (lay->fade < lay->end - cur) {
        //perform fade out
        for (uint32_t i = 0; i < asize; i += 2) {
            //quit if fully faded out
            if (lay->fade == 0) break;
            //mix if cursor within sound
            if (cur >= 0) {
                //get faded volume multiplier
                __m128 fmul = _mm_mul_ps(_mm_set_ps1((float)lay->fade/(float)lay->fmax), gmul);
                //mod for repeating and convert to __m128 offset
                int32_t off = (cur % lay->snd->len) >> 1;
                //mix in first two frames
                align[i] = _mm_add_ps(align[i], _mm_mul_ps(lay->snd->data[off], fmul));
                //mix in second two frames
                align[i+1] = _mm_add_ps(align[i+1], _mm_mul_ps(lay->snd->data[off+1], fmul));
            }
            //advance cursor and fade
            lay->fade -= 4; cur += 4;
        }
    } else {
        //continue playback to end without fade out
        for (uint32_t i = 0; i < asize; i += 2) {
            //quit if cursor at end
            if (cur == lay->end) break;
            //mix if cursor within sound
            if (cur >= 0) {
                //mod for repeating and convert to __m128 offset
                int32_t off = (cur % lay->snd->len) >> 1;
                //mix in first two frames
                align[i] = _mm_add_ps(align[i], _mm_mul_ps(lay->snd->data[off], gmul));
                //mix in second two frames
                align[i+1] = _mm_add_ps(align[i+1], _mm_mul_ps(lay->snd->data[off+1], gmul));
            }
            //advance cursor
            cur += 4;
        }
    }
    //swap back cursor if unchanged
    if (!ATMX_CSWAP(&lay->cursor, &old, cur)) cur = old;
    //return new cursor
    return cur;
}
static int32_t atmxMixPlayMono (struct atmx_layer* lay, int loop, int32_t cur, __m128 gmul, __m128* align, uint32_t asize) {
    //cache cursor
    int32_t old = cur;
    //check if fully faded in yet
    if (lay->fade < lay->fmax) {
        //perform fade in
        for (uint32_t i = 0; i < asize; i += 2) {
            //check if cursor at end
            if (cur == lay->end) {
                //quit unless looping
                if (!loop) break;
                //wrap around if looping
                cur = lay->start;
            }
            //mix if cursor within sound
            if (cur >= 0) {
                //get faded volume multiplier
                __m128 fmul = _mm_mul_ps(_mm_set_ps1((float)lay->fade/(float)lay->fmax), gmul);
                //load 4 samples from data (this is 4 frames)
                __m128 sam = lay->snd->data[(cur % lay->snd->len) >> 2];
                //mix low samples obtained with unpacklo
                align[i] = _mm_add_ps(align[i], _mm_mul_ps(_mm_unpacklo_ps(sam, sam), fmul));
                //mix high samples obtained with unpackhi
                align[i+1] = _mm_add_ps(align[i+1], _mm_mul_ps(_mm_unpackhi_ps(sam, sam), fmul));
            }
            //advance fade unless fully faded in
            if (lay->fade < lay->fmax) lay->fade += 4;
            //advance cursor
            cur += 4;
        }
    } else {
        //regular playback
        for (uint32_t i = 0; i < asize; i += 2) {
            //check if cursor at end
            if (cur == lay->end) {
                //quit unless looping
                if (!loop) break;
                //wrap around if looping
                cur = lay->start;
            }
            //mix if cursor within sound
            if (cur >= 0) {
                //load 4 samples from data (this is 4 frames)
                __m128 sam = lay->snd->data[(cur % lay->snd->len) >> 2];
                //mix low samples obtained with unpacklo
                align[i] = _mm_add_ps(align[i], _mm_mul_ps(_mm_unpacklo_ps(sam, sam), gmul));
                //mix high samples obtained with unpackhi
                align[i+1] = _mm_add_ps(align[i+1], _mm_mul_ps(_mm_unpackhi_ps(sam, sam), gmul));
            }
            //advance cursor
            cur += 4;
        }
    }
    //swap back cursor if unchanged
    if (!ATMX_CSWAP(&lay->cursor, &old, cur)) cur = old;
    //return new cursor
    return cur;
}
static int32_t atmxMixPlayStereo (struct atmx_layer* lay, int loop, int32_t cur, __m128 gmul, __m128* align, uint32_t asize) {
    //cache cursor
    int32_t old = cur;
    //check if fully faded in yet
    if (lay->fade < lay->fmax) {
        //perform fade in
        for (uint32_t i = 0; i < asize; i += 2) {
            //check if cursor at end
            if (cur == lay->end) {
                //quit unless looping
                if (!loop) break;
                //wrap around if looping
                cur = lay->start;
            }
            //mix if cursor within sound
            if (cur >= 0) {
                //get faded volume multiplier
                __m128 fmul = _mm_mul_ps(_mm_set_ps1((float)lay->fade/(float)lay->fmax), gmul);
                //mod for repeating and convert to __m128 offset
                int32_t off = (cur % lay->snd->len) >> 1;
                //mix in first two frames
                align[i] = _mm_add_ps(align[i], _mm_mul_ps(lay->snd->data[off], fmul));
                //mix in second two frames
                align[i+1] = _mm_add_ps(align[i+1], _mm_mul_ps(lay->snd->data[off+1], fmul));
            }
            //advance fade unless fully faded in
            if (lay->fade < lay->fmax) lay->fade += 4;
            //advance cursor
            cur += 4;
        }
    } else {
        //regular playback
        for (uint32_t i = 0; i < asize; i += 2) {
            //check if cursor at end
            if (cur == lay->end) {
                //quit unless looping
                if (!loop) break;
                //wrap around if looping
                cur = lay->start;
            }
            //mix if cursor within sound
            if (cur >= 0) {
                //mod for repeating and convert to __m128 offset
                int32_t off = (cur % lay->snd->len) >> 1;
                //mix in first two frames
                align[i] = _mm_add_ps(align[i], _mm_mul_ps(lay->snd->data[off], gmul));
                //mix in second two frames
                align[i+1] = _mm_add_ps(align[i+1], _mm_mul_ps(lay->snd->data[off+1], gmul));
            }
            //advance cursor
            cur += 4;
        }
    }
    //swap back cursor if unchanged
    if (!ATMX_CSWAP(&lay->cursor, &old, cur)) cur = old;
    //return new cursor
    return cur;
}
#else
static void atmxMixLayer (struct atmx_layer* lay, float vol, float* buff, uint32_t fnum) {
    //load flag value atomically first
    uint8_t flag = ATMX_LOAD(&lay->flag);
    //return if flag cleared
    if (flag == 0) return;
    //atomically load cursor
    int32_t cur = ATMX_LOAD(&lay->cursor);
    //atomically load left and right gain
    struct atmx_f2 g = ATMX_LOAD(&lay->gain);
    //multiply volume into gain
    g.l *= vol; g.r *= vol;
    //action based on flag
    if (flag < 3) {
        //ATOMIX_STOP or ATOMIX_HALT, fade out if not faded or at end
        if ((lay->fade > 0)&&(cur < lay->end)) 
            if (lay->snd->cha == 1)
                cur = atmxMixFadeMono(lay, cur, g, buff, fnum);
            else
                cur = atmxMixFadeStereo(lay, cur, g, buff, fnum);
        //clear flag if ATOMIX_STOP and fully faded or at end
        if ((flag == ATOMIX_STOP)&&((lay->fade == 0)||(cur == lay->end))) ATMX_STORE(&lay->flag, 0);
    } else {
        //ATOMIX_PLAY or ATOMIX_LOOP, play including fade in
        if (lay->snd->cha == 1)
            cur = atmxMixPlayMono(lay, (flag == ATOMIX_LOOP), cur, g, buff, fnum);
        else
            cur = atmxMixPlayStereo(lay, (flag == ATOMIX_LOOP), cur, g, buff, fnum);
        //clear flag if ATOMIX_PLAY and the cursor has reached the end
        if ((flag == ATOMIX_PLAY)&&(cur == lay->end)) ATMX_CSWAP(&lay->flag, &flag, 0);
    }
}
static int32_t atmxMixFadeMono (struct atmx_layer* lay, int32_t cur, struct atmx_f2 g, float* buff, uint32_t fnum) {
    //cache cursor
    int32_t old = cur;
    //check if enough samples left for fade out
    if (lay->fade < lay->end - cur) {
        //perform fade out
        for (uint32_t i = 0; i < fnum*2; i += 2) {
            //quit if fully faded out
            if (lay->fade == 0) break;
            //mix if cursor within sound
            if (cur >= 0) {
                //get fade multiplier
                float fade = (float)lay->fade/(float)lay->fmax;
                //load 1 sample from data (this is 1 frame)
                float sam = lay->snd->data[cur % lay->snd->len];
                //mix left sample of frame
                buff[i] += sam*fade*g.l;
                //mix right sample of frame
                buff[i+1] += sam*fade*g.r;
            }
            //advance cursor and fade
            lay->fade--; cur++;
        }
    } else {
        //continue playback to end without fade out
        for (uint32_t i = 0; i < fnum*2; i += 2) {
            //quit if cursor at end
            if (cur == lay->end) break;
            //mix if cursor within sound
            if (cur >= 0) {
                //load 1 sample from data (this is 1 frame)
                float sam = lay->snd->data[cur % lay->snd->len];
                //mix left sample of frame
                buff[i] += sam*g.l;
                //mix right sample of frame
                buff[i+1] += sam*g.r;
            }
            //advance cursor
            cur++;
        }
    }
    //swap back cursor if unchanged
    if (!ATMX_CSWAP(&lay->cursor, &old, cur)) cur = old;
    //return new cursor
    return cur;
}
static int32_t atmxMixFadeStereo (struct atmx_layer* lay, int32_t cur, struct atmx_f2 g, float* buff, uint32_t fnum) {
    //cache cursor
    int32_t old = cur;
    //check if enough samples left for fade out
    if (lay->fade < lay->end - cur) {
        //perform fade out
        for (uint32_t i = 0; i < fnum*2; i += 2) {
            //quit if fully faded out
            if (lay->fade == 0) break;
            //mix if cursor within sound
            if (cur >= 0) {
                //get fade multiplier
                float fade = (float)lay->fade/(float)lay->fmax;
                //mod for repeating and convert to float offset
                int32_t off = (cur % lay->snd->len) << 1;
                //mix left sample of frame
                buff[i] += lay->snd->data[off]*fade*g.l;
                //mix right sample of frame
                buff[i+1] += lay->snd->data[off+1]*fade*g.r;
            }
            //advance cursor and fade
            lay->fade--; cur++;
        }
    } else {
        //continue playback to end without fade out
        for (uint32_t i = 0; i < fnum*2; i += 2) {
            //quit if cursor at end
            if (cur == lay->end) break;
            //mix if cursor within sound
            if (cur >= 0) {
                //mod for repeating and convert to float offset
                int32_t off = (cur % lay->snd->len) << 1;
                //mix left sample of frame
                buff[i] += lay->snd->data[off]*g.l;
                //mix right sample of frame
                buff[i+1] += lay->snd->data[off+1]*g.r;
            }
            //advance cursor
            cur++;
        }
    }
    //swap back cursor if unchanged
    if (!ATMX_CSWAP(&lay->cursor, &old, cur)) cur = old;
    //return new cursor
    return cur;
}
static int32_t atmxMixPlayMono (struct atmx_layer* lay, int loop, int32_t cur, struct atmx_f2 g, float* buff, uint32_t fnum) {
    //cache cursor
    int32_t old = cur;
    //check if fully faded in yet
    if (lay->fade < lay->fmax) {
        //perform fade in
        for (uint32_t i = 0; i < fnum*2; i += 2) {
            //check if cursor at end
            if (cur == lay->end) {
                //quit unless looping
                if (!loop) break;
                //wrap around if looping
                cur = lay->start;
            }
            //mix if cursor within sound
            if (cur >= 0) {
                //get fade multiplier
                float fade = (float)lay->fade/(float)lay->fmax;
                //load 1 sample from data (this is 1 frame)
                float sam = lay->snd->data[cur % lay->snd->len];
                //mix left sample of frame
                buff[i] += sam*fade*g.l;
                //mix right sample of frame
                buff[i+1] += sam*fade*g.r;
            }
            //advance fade unless fully faded in
            if (lay->fade < lay->fmax) lay->fade++;
            //advance cursor
            cur++;
        }
    } else {
        //regular playback
        for (uint32_t i = 0; i < fnum*2; i += 2) {
            //check if cursor at end
            if (cur == lay->end) {
                //quit unless looping
                if (!loop) break;
                //wrap around if looping
                cur = lay->start;
            }
            //mix if cursor within sound
            if (cur >= 0) {
                //load 1 sample from data (this is 1 frame)
                float sam = lay->snd->data[cur % lay->snd->len];
                //mix left sample of frame
                buff[i] += sam*g.l;
                //mix right sample of frame
                buff[i+1] += sam*g.r;
            }
            //advance cursor
            cur++;
        }
    }
    //swap back cursor if unchanged
    if (!ATMX_CSWAP(&lay->cursor, &old, cur)) cur = old;
    //return new cursor
    return cur;
}
static int32_t atmxMixPlayStereo (struct atmx_layer* lay, int loop, int32_t cur, struct atmx_f2 g, float* buff, uint32_t fnum) {
    //cache cursor
    int32_t old = cur;
    //check if fully faded in yet
    if (lay->fade < lay->fmax) {
        //perform fade in
        for (uint32_t i = 0; i < fnum*2; i += 2) {
            //check if cursor at end
            if (cur == lay->end) {
                //quit unless looping
                if (!loop) break;
                //wrap around if looping
                cur = lay->start;
            }
            //mix if cursor within sound
            if (cur >= 0) {
                //get fade multiplier
                float fade = (float)lay->fade/(float)lay->fmax;
                //mod for repeating and convert to float offset
                int32_t off = (cur % lay->snd->len) << 1;
                //mix left sample of frame
                buff[i] += lay->snd->data[off]*fade*g.l;
                //mix right sample of frame
                buff[i+1] += lay->snd->data[off+1]*fade*g.r;
            }
            //advance fade unless fully faded in
            if (lay->fade < lay->fmax) lay->fade++;
            //advance cursor
            cur++;
        }
    } else {
        //regular playback
        for (uint32_t i = 0; i < fnum*2; i += 2) {
            //check if cursor at end
            if (cur == lay->end) {
                //quit unless looping
                if (!loop) break;
                //wrap around if looping
                cur = lay->start;
            }
            //mix if cursor within sound
            if (cur >= 0) {
                //mod for repeating and convert to float offset
                int32_t off = (cur % lay->snd->len) << 1;
                //mix left sample of frame
                buff[i] += lay->snd->data[off]*g.l;
                //mix right sample of frame
                buff[i+1] += lay->snd->data[off+1]*g.r;
            }
            //advance cursor
            cur++;
        }
    }
    //swap back cursor if unchanged
    if (!ATMX_CSWAP(&lay->cursor, &old, cur)) cur = old;
    //return new cursor
    return cur;
}
#endif
static struct atmx_f2 atmxGainf2 (float gain, float pan) {
    //clamp pan to its valid range of -1.0f to 1.0f inclusive
    pan = (pan < -1.0f) ? -1.0f : (pan > 1.0f) ? 1.0f : pan;
    //convert gain and pan to left and right gain and store it atomically
    return (struct atmx_f2){gain*(0.5f - pan/2.0f), gain*(0.5f + pan/2.0f)};
}

#endif //ATOMIX_IMPLEMENTATION