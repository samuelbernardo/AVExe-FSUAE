#ifdef USE_OPENAL

#include <fs/emu.h>
#include <stdio.h>
#ifdef MACOSX
#include <al.h>
#include <alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif
#include <fs/base.h>
#include <fs/queue.h>
#include <fs/thread.h>
#include "libfsemu.h"
#include "audio.h"

// the actual frequency negotiated with the audio driver
static double g_audio_out_frequency = 0.0;

static int g_volume = 100;
static int g_mute = 0;
static double g_want_volume = 1.0;
static int g_sys_buffer_bytes = 0;

double g_default_audio_pitch = 1.0;

void fs_emu_audio_set_default_pitch(double pitch) {
    g_default_audio_pitch = pitch;
}

int g_fs_emu_audio_stream_playing[MAX_STREAMS] = {};

#define FRAME_TIME_LIST_COUNT 256
#define AUDIO_AVG_FILL_QUEUE_COUNT 256
#define AUDIO_FILL_QUEUE_COUNT 256
#define AUDIO_FILL_QUEUE2_COUNT 128
#define AUDIO_IN_FRAME_QUEUE_SIZE 128
#define AUDIO_OUT_FRAME_QUEUE_SIZE 1024

#define FILL_STAT_BUFFER_LENGTH 512
#define FILL_STAT_BUFFER_ITEMS 256

#define FILL_STAT_BUFFER_ITEMS_AVG 64
#define FILL_STAT_BUFFER_ITEMS_MIN 64
#define FILL_STAT_BUFFER_ITEMS_ERR 64

#define MEMORY_BARRIER __asm__ __volatile__ ("" ::: "memory")

typedef struct audio_stream {
    ALuint source;
    ALuint *buffers;
    int num_buffers;
    int buffer_size;
    int frequency;
    fs_queue *queue;
    fs_mutex *mutex;
    int buffers_queued;
    int min_buffers;
    int fill_target;

    int fill_stat_buffer[FILL_STAT_BUFFER_LENGTH];

    int err_stat_buffer[FILL_STAT_BUFFER_LENGTH];
    int err_stat_buffer_sum;

    int avg_fill_stat_buffer[FILL_STAT_BUFFER_LENGTH];
    int min_fill_stat_buffer[FILL_STAT_BUFFER_LENGTH];
    int avg_min_fill_stat_buffer[FILL_STAT_BUFFER_LENGTH];

    int fill_stat_buffer_avg_sum;
    int fill_stat_buffer_avg;
    int fill_stat_buffer_min;
    int fill_stat_buffer_avg_min_sum;
    int fill_stat_buffer_avg_min;
    volatile int fill_stat_buffer_pos;

    //double pid_integral;
    double pid_last_error;
    double pid_last_last_error;
    int pid_last_time;
    int pid_last_last_time;
    double pitch;

    double source_volume_current;

} audio_stream;

static audio_stream *g_streams[MAX_STREAMS] = {};

#define MAX_BUFFERS 48

static ALCdevice *g_device = NULL;
static ALCcontext *g_context = NULL;

static int check_al_error(const char *label) {
    int error;
    if ((error = alGetError()) != AL_NO_ERROR) {
        fs_log("openal error %d (%s)\n", error, label);
    }
    return error;
}

#define KP 0.0000005
#define KI 0.00000000001
#define KD 0.0000000001

static void pid_controller_step(int stream, int time_ms) {
    audio_stream *s = g_streams[stream];
    if (s->fill_target == 0) {
        return;
    }
    if (s->pid_last_last_time == 0) {
        s->pid_last_time = time_ms;
        s->pid_last_last_time = time_ms;
    }
    int dt = time_ms - s->pid_last_last_time;
    int error = s->fill_target - s->fill_stat_buffer_avg;

    //static double last_error = 0.0;
    //if (s->pid_last_error == 0.0) {
    //    s->pid_last_error = error;
    //}
    //s->pid_integral = s->pid_integral + (error * dt);
    //s->pid_integral = s->err_stat_buffer_sum;

    double derivative;
    if (dt > 0) {
        derivative = (error - s->pid_last_last_error) / dt;
    }
    else {
        derivative = 0;
    }
    double output = KP * error + KI * s->err_stat_buffer_sum + KD * derivative;

    s->pid_last_last_error = s->pid_last_error;
    s->pid_last_last_time = s->pid_last_time ;
    s->pid_last_error = error;
    s->pid_last_time = time_ms;

    s->pitch = g_default_audio_pitch - output;
    s->pitch = MAX(0.5, MIN(2.0, s->pitch));
#if 0
    //s->pitch = 1.0;
    static int count = 0;
    if (++count % 100 == 0) {
        printf("pitch: %0.2f\n", s->pitch);
    }
#endif
    alSourcef(s->source, AL_PITCH, (ALfloat) s->pitch);
    check_al_error("alSourcef (AL_PITCH)");
}

static void unqueue_old_buffers(int stream) {
    audio_stream *s = g_streams[stream];
    ALint old_buffers = 0;
    fs_mutex_lock(s->mutex);
    // locking here because unqueue_old_buffers can be run called from
    // both the video thread and the emulation thread (consider changing this,
    // perhaps even have a separate thread for periodically unqueuing).
    alGetSourcei(s->source, AL_BUFFERS_PROCESSED, &old_buffers);
    check_al_error("alGetSourcei (AL_BUFFERS_PROCESSED)");

    if (old_buffers > 0) {
        ALuint buffers[MAX_BUFFERS];
        old_buffers = MIN(old_buffers, MAX_BUFFERS);
        alSourceUnqueueBuffers(s->source, old_buffers, buffers);
        if (check_al_error("alSourceUnqueueBuffers") != AL_NO_ERROR) {
            fs_log("while trying to unqueue %d buffers\n");
        }
        for (int i = 0; i < old_buffers; i++) {
            fs_queue_push_tail(s->queue, FS_UINT_TO_POINTER(buffers[i]));
        }
        s->buffers_queued -= old_buffers;
    }
    fs_mutex_unlock(s->mutex);
}

void fs_emu_audio_pause_stream(int stream) {
    fs_log("fs_emu_audio_resume_stream %d\n", stream);
    audio_stream *s = g_streams[stream];
    alSourcePause(s->source);
    g_fs_emu_audio_stream_playing[stream] = 0;
    check_al_error("alSourcePlay");
}

void fs_emu_audio_resume_stream(int stream) {
    fs_log("fs_emu_audio_resume_stream %d\n", stream);
    audio_stream *s = g_streams[stream];
    alSourcePlay(s->source);
    g_fs_emu_audio_stream_playing[stream] = 1;
    check_al_error("alSourcePlay");
}


int fs_emu_check_audio_buffer_done(int stream, int buffer) {
    unqueue_old_buffers(stream);
    audio_stream *s = g_streams[stream];
    // not extremely efficient
    fs_mutex_lock(s->mutex);
    fs_list *link = fs_queue_peek_head_link(s->queue);
    while (link) {
        if ((unsigned int) buffer == FS_POINTER_TO_UINT(link->data)) {
            fs_mutex_unlock(s->mutex);
            return 1;
        }
        link = link->next;
    }
    fs_mutex_unlock(s->mutex);
    return 0;
}

int fs_emu_queue_audio_buffer(int stream, int16_t* data, int size) {
    if (g_fs_emu_benchmarking) {
        // no audio output while benchmarking
        return 0;
    }
    //fs_log("fs_emu_queue_audio_buffer stream %d size %d\n", stream, size);
    audio_stream *s = g_streams[stream];

    ALuint buffer = 0;
    fs_mutex_lock(s->mutex);
    //while (1) {
    buffer = FS_POINTER_TO_UINT(fs_queue_pop_head(s->queue));
    if (!buffer) {
        fs_log("no audio buffer available - dropping data\n");
        fs_mutex_unlock(s->mutex);
        return 0;
    }
    s->buffers_queued += 1;
    // create a local copy while we have the lock
    //int buffers_queued = s->buffers_queued;
    fs_mutex_unlock(s->mutex);

    // for debugging, clear one of the stereo channels
    //int16_t *d = data;
    //for (int i = 0; i < size / 4; i++) {
    //    d++;
    //    *d = 0;
    //    d++;
    //}

    alBufferData(buffer, AL_FORMAT_STEREO16, data, size, s->frequency);
    check_al_error("alBufferData");
    alSourceQueueBuffers(s->source, 1, &buffer);
    check_al_error("alSourceQueueBuffers");

    ALint state;
    alGetSourcei(s->source, AL_SOURCE_STATE, &state);
    check_al_error("alGetSourcei (AL_SOURCE_STATE)");
    if (state != AL_PLAYING) {
        g_fs_emu_audio_buffer_underrun_time = fs_get_monotonic_time();
        g_fs_emu_audio_buffer_underruns += 1;
        // we have had a buffer underrun - we now wait until we have queued
        // some buffers
        //if (buffers_queued < s->min_buffers) {
        //    // want more buffers
        //}
        //else {
            fs_log("restarting audio stream %d (buffer underrun)\n", stream);
            alSourcePlay(s->source);
            g_fs_emu_audio_stream_playing[stream] = 1;
            check_al_error("alSourcePlay");
            //}
    }

    double want_volume = g_want_volume * 0.9;
    if (want_volume != s->source_volume_current) {
        s->source_volume_current = want_volume;
        alSourcef(s->source, AL_GAIN, want_volume);
    }

    unqueue_old_buffers(stream);
    return buffer;
}

double fs_emu_audio_get_measured_avg_buffer_fill(int stream) {
    audio_stream *s = g_streams[stream];
    return s->fill_stat_buffer_avg;
}

void fs_emu_enable_audio_stream(int stream) {
    fs_log("enabling audio stream %d\n", stream);
}

void fs_emu_disable_audio_stream(int stream) {
    fs_log("disabling audio stream %d\n", stream);
}

void fs_emu_audio_set_volume(int volume) {
    g_volume = volume;
    g_want_volume = g_mute ? 0.0 : g_volume / 100.0;
}

void fs_emu_audio_set_mute(int mute) {
    g_mute = mute;
    g_want_volume = g_mute ? 0.0 : g_volume / 100.0;
}

int fs_emu_audio_get_volume() {
    return g_volume;
}

int fs_emu_audio_get_mute() {
    return g_mute || g_volume == 0;
}

void fs_emu_set_audio_buffer_frequency(int stream, int frequency) {

}

int fs_emu_get_audio_frequency() {
    return g_audio_out_frequency;
}

int fs_emu_get_audio_buffer_size() {
    return g_sys_buffer_bytes;
}

void fs_emu_audio_sample(int stream, int16_t left, int16_t right) {

}


static void update_stats(int stream, int time_ms) {
    if (!g_fs_emu_audio_stream_playing[stream]) {
        return;
    }
    audio_stream *s = g_streams[stream];
    int available = s->buffers_queued * s->buffer_size;
    //int error = available - s->fill_target;
    int error = s->fill_target - s->fill_stat_buffer_avg;

    int head_pos = s->fill_stat_buffer_pos;
    int avg_tail_pos = head_pos - (FILL_STAT_BUFFER_ITEMS_AVG - 1);
    int min_tail_pos = head_pos - (FILL_STAT_BUFFER_ITEMS_MIN - 1);
    head_pos = (head_pos + 1) % FILL_STAT_BUFFER_LENGTH;
    avg_tail_pos = (avg_tail_pos + FILL_STAT_BUFFER_LENGTH) %
            FILL_STAT_BUFFER_LENGTH;
    min_tail_pos = (min_tail_pos + FILL_STAT_BUFFER_LENGTH) %
            FILL_STAT_BUFFER_LENGTH;

    s->fill_stat_buffer[head_pos] = available;

    s->err_stat_buffer[head_pos] = error;
    int err_tail_pos = head_pos - (FILL_STAT_BUFFER_ITEMS_ERR - 1);
    err_tail_pos = (err_tail_pos + FILL_STAT_BUFFER_LENGTH) %
            FILL_STAT_BUFFER_LENGTH;
    s->err_stat_buffer_sum = s->err_stat_buffer_sum -
            s->err_stat_buffer[err_tail_pos] + error;

    s->fill_stat_buffer_avg_sum = s->fill_stat_buffer_avg_sum - \
            s->fill_stat_buffer[avg_tail_pos] +  available;
    s->fill_stat_buffer_avg = s->fill_stat_buffer_avg_sum /
            FILL_STAT_BUFFER_ITEMS_AVG;
    //printf("%d %d\n", available, s->fill_stat_buffer_avg);

    if (s->fill_stat_buffer[min_tail_pos] <= s->fill_stat_buffer_min) {
        if (s->fill_stat_buffer[min_tail_pos] < s->fill_stat_buffer_min) {
            fs_log("\n\nERROR WITH g_fill_stat_buffer_min, "
                    "g_fill_stat_buffer[min_tail_pos] = %d, "
                    "g_fill_stat_buffer_min = %d\n\n",
                    s->fill_stat_buffer[min_tail_pos],
                    s->fill_stat_buffer_min);
        }
        // must re-calculate min value
        int pos = head_pos;
        int min = 1000000000;
        for (int i = 0; i < FILL_STAT_BUFFER_ITEMS_MIN; i++) {
            int val = s->fill_stat_buffer[pos];
            if (val < min) {
                min = val;
            }
            pos = pos - 1;
            pos = (pos + FILL_STAT_BUFFER_LENGTH) % FILL_STAT_BUFFER_LENGTH;
        }
        s->fill_stat_buffer_min = min;
    }

    s->fill_stat_buffer_avg_min_sum = s->fill_stat_buffer_avg_min_sum - \
            s->min_fill_stat_buffer[avg_tail_pos] + s->fill_stat_buffer_min;
    s->fill_stat_buffer_avg_min = s->fill_stat_buffer_avg_min_sum /
            FILL_STAT_BUFFER_ITEMS_AVG;

    s->avg_fill_stat_buffer[head_pos] = s->fill_stat_buffer_avg;
    s->min_fill_stat_buffer[head_pos] = s->fill_stat_buffer_min;
    s->avg_min_fill_stat_buffer[head_pos] = s->fill_stat_buffer_avg_min;

    MEMORY_BARRIER;
    //g_fill_stat_buffer_pos = (g_fill_stat_buffer_pos + 1) % FILL_STAT_BUFFER_LENGTH;
    s->fill_stat_buffer_pos = head_pos;
}

void fs_emu_audio_video_sync(int time_ms) {
    for (int i = 0; i < MAX_STREAMS; i++) {
        if (g_streams[i]) {
            unqueue_old_buffers(i);
            update_stats(i, time_ms);
            pid_controller_step(i, time_ms);
        }
    }
}

static void log_openal_info() {
    if (alGetString(AL_VERSION)) {
        fs_log("openal version: %s\n", alGetString(AL_VERSION));
    }
    if (alGetString(AL_RENDERER)) {
        fs_log("openal renderer: %s\n", alGetString(AL_RENDERER));
    }
    if (alGetString(AL_VENDOR)) {
        fs_log("openal vendor: %s\n", alGetString(AL_VENDOR));
    }
    if (alGetString(AL_EXTENSIONS)) {
        fs_log("openal extensions: %s\n", alGetString(AL_EXTENSIONS));
    }

    // enumerate OpenAL devices
    fs_log("openal devices:\n");
    if (alcIsExtensionPresent (NULL, "ALC_ENUMERATION_EXT") == AL_TRUE) {
        const char *s = (const char *) alcGetString(NULL,
                ALC_DEVICE_SPECIFIER);
        while (*s) {
            fs_log("- %s\n", s);
            while (*s++) {
                // loop until s points to next string
            }
        }
    }
    else {
        fs_log(" - no support for device enumeration\n");
    }

    // print default device name
    fs_log("openal default device: %s\n",
            alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER));
}


static void openal_audio_init() {
    fs_log("openal_audio_init\n");

    int volume = fs_config_get_int_clamped("volume", 0, 100);
    if (volume != FS_CONFIG_NONE) {
        if (volume == 0) {
            fs_emu_audio_set_mute(1);
        }
        else {
            fs_emu_audio_set_volume(volume);
        }
    }

    // select the "preferred device"
    g_device = alcOpenDevice(NULL);
    if (g_device) {
        fs_log("openal device opened: %s\n",
                alcGetString(g_device, ALC_DEVICE_SPECIFIER));

    }
    else {
        fs_log("alcOpenDevice returned NULL\n");
        fs_emu_warning("OpenAL: Could not open audio device device\n");
        ALenum error_code = alGetError();
        fs_log("OpenAL error code: %d\n", error_code);
        if (alGetString(error_code)) {
            fs_log("(%s)\n", alGetString(error_code));
        }
    }

    log_openal_info();
    if (!g_device) {
        return;
    }

    int frequencies[] = { 48000, 44100 };
    if (fs_config_get_int("audio_frequency") != FS_CONFIG_NONE) {
        frequencies[0] = fs_config_get_int("audio_frequency");
    }

    for (int i = 0; i < 3; i++) {
        int frequency = frequencies[i];
        fs_log("openal: trying frequency %d\n", frequency);
        ALCint attributes[] = {
            ALC_MONO_SOURCES, 0,
            ALC_STEREO_SOURCES, 2,
            ALC_FREQUENCY, frequency,
            0 };
#if 0
        if (frequency == 0) {
            // this will zero out ALC_FREQUENCY, so this index will be
            // interpreted as end-of-attributes and OpenAL will choose
            // frequency itself.
            attributes[4] = 0;
        }
#endif

        g_context = alcCreateContext(g_device, attributes);
        if (g_context) {
            g_audio_out_frequency = frequency;
            break;
        }
    }

#if 0
    // FIXME: enable later
    ALCint attributes[3] = { ALC_FREQUENCY,  48000, 0 };

    fs_log("openal: trying 48000\n");
    g_context = alcCreateContext(g_device, attributes);
    if (g_context) {
        g_audio_out_frequency = 48000;
    }
    else {
        //check_al_error("alcCreateContext");
        fs_log("openal: trying without frequency specified\n");
   }
#endif
#if 0
    ALCint attributes[] = { ALC_MONO_SOURCES, 0,
            ALC_STEREO_SOURCES, 2, 0 };

    if (!g_context) {
        g_context = alcCreateContext(g_device, attributes);
        g_audio_out_frequency = 44100;
    }
#endif
    if (g_context) {
        fs_log("openal: created context\n");
        alcMakeContextCurrent(g_context);
        check_al_error("alcMakeContextCurrent");
        fs_log("openal: made context current\n");
    }
    else {
        fs_emu_warning("OpenAL: no context created\n");
        //check_al_error("alcCreateContext");
    }

    int stereo_sources;
    alcGetIntegerv(g_device, ALC_STEREO_SOURCES, 1, &stereo_sources);
    fs_log("openal: number of stereo sources is %d\n", stereo_sources);
}

void fs_emu_audio_init() {
    fs_log("fs_emu_audio_init\n");
    openal_audio_init();
}

static void openal_audio_shutdown() {
    //alDeleteSources(NUM_SOURCES, source);
    //alDeleteBuffers(NUM_BUFFERS, buffers);
    alcMakeContextCurrent(NULL);
    if (!g_context) {
        alcDestroyContext(g_context);
        g_context = NULL;
    }
    if (g_device) {
        alcCloseDevice(g_device);
        g_device = NULL;
    }
}

void fs_emu_audio_shutdown() {
    openal_audio_shutdown();
}

void fs_emu_init_audio_stream_options(fs_emu_audio_stream_options *options) {
    options->frequency = 44100;
    options->channels = 2;
    options->sample_size = 16;
    //options->buffer_size = 1024;
    options->buffer_size = 512;
    //options->min_buffers = 10;
    options->min_buffers = 1;
}

void fs_emu_init_audio_stream(int stream,
        fs_emu_audio_stream_options *options) {
    fs_log("initializing audio stream %d\n", stream);
    audio_stream *s = fs_malloc0(sizeof(audio_stream));
    s->buffer_size = options->buffer_size;
    s->frequency = options->frequency;
    s->num_buffers = MAX_BUFFERS;
    s->min_buffers = options->min_buffers;
    fs_log("frequency: %d, buffers: %d buffer size: %d bytes\n",
            s->frequency, s->num_buffers, s->buffer_size);
    s->mutex = fs_mutex_create();
    s->queue = fs_queue_new();
    s->source_volume_current = 1.0;
    alGenSources(1, &s->source);

    //alSourcei (s->source, AL_SOURCE_RELATIVE, AL_TRUE);
    //alSource3f(s->source, AL_POSITION, 0.0, 0.0, -1.0);
    //alSourcef (s->source, AL_ROLLOFF_FACTOR, 0.0);

    // AL_DIRECT_CHANNELS_SOFT
    alSourcei (s->source, 0x1033, AL_TRUE);

    check_al_error("alGenSources");
    for (int i = 0; i < s->num_buffers; i++) {
        ALuint buffer;
        alGenBuffers(1, &buffer);
        check_al_error("alGenBuffers");
        fs_queue_push_tail(s->queue, FS_UINT_TO_POINTER(buffer));
    }
    s->buffers_queued = 0;

    // FIXME: configure elsewhere
    if (stream == 0) {
        int abt = fs_config_get_int_clamped("audio_buffer_target_size",
                1, 100);
        if (abt == FS_CONFIG_NONE) {
            abt = fs_config_get_int_clamped("audio_buffer_target_bytes",
                    512, 16 * 1024);
            if (abt == FS_CONFIG_NONE) {
                abt = 40;
            }
        }
        if (abt <= 100) {
            fs_log("audio buffer target size (ms) = %d\n", abt);
            abt = abt / 1000.0 * (options->frequency * 2 * 2);
            fs_log("audio buffer target size (bytes) = %d\n", abt);
        }
        s->fill_target = abt;
    }
    else {
        s->fill_target = 0;
    }
    s->pid_last_error = 0;
    s->pid_last_last_error = 0;
    s->pid_last_time = 0;
    s->pid_last_last_time = 0;
    g_streams[stream] = s;
}

#define AUDIO_DEBUG_SCALE 0.012

void fs_emu_audio_render_debug_info(uint32_t *texture) {
    int x;
    int y1;
    uint32_t color;
    uint32_t *line;

    audio_stream *s = g_streams[0];
    int head_pos = s->fill_stat_buffer_pos;

    int pos = head_pos;

    x = 255;
    y1 = 0;
    for (int i = 0; i < FILL_STAT_BUFFER_ITEMS; i++) {
        int val = s->fill_stat_buffer[pos];
        int underrun = val < g_sys_buffer_bytes;
        int y2 = y1 + val * AUDIO_DEBUG_SCALE;
        y2 = MIN(y2, 255);
        if (underrun) {
            color = 0x80000080;
        }
        else {
            color = 0x60602020;
        }
        for (int y = y1; y < y2; y++) {
            *(texture + ((y * 256) + x)) = color;
        }
        if (--x < 128) {
            break;
        }
        pos = pos - 1;
        pos = (pos + FILL_STAT_BUFFER_LENGTH) % FILL_STAT_BUFFER_LENGTH;
    }

    pos = head_pos;
    color = 0xb0b0b0b0;
    x = 255;
    for (int i = 0; i < FILL_STAT_BUFFER_ITEMS; i++) {
        int val = s->avg_fill_stat_buffer[pos];
        int y = val * AUDIO_DEBUG_SCALE;
        if (--x < 128) {
            break;
        }
        y = MIN(y, 255);
        *(texture + ((y * 256) + x)) = color;
        pos = pos - 1;
        pos = (pos + FILL_STAT_BUFFER_LENGTH) % FILL_STAT_BUFFER_LENGTH;

    }

    pos = head_pos;
    color = 0xb000b0b0;
    x = 255;
    for (int i = 0; i < FILL_STAT_BUFFER_ITEMS; i++) {
        int val = s->avg_min_fill_stat_buffer[pos];
        int y = val * AUDIO_DEBUG_SCALE;
        if (--x < 128) {
            break;
        }
        y = MIN(y, 255);
        *(texture + ((y * 256) + x)) = color;
        pos = pos - 1;
        pos = (pos + FILL_STAT_BUFFER_LENGTH) % FILL_STAT_BUFFER_LENGTH;

    }

    // target buffer line
    color = 0xb000b000;
    line = texture + (int) (s->fill_target * AUDIO_DEBUG_SCALE) * 256 +
            128;
    for (int x = 128; x < 256; x++) {
        *(line++) = color;
    }
}

fs_emu_audio_driver g_fs_emu_audio_openal_driver = {
    "openal",
    openal_audio_init,
    openal_audio_shutdown,
};

#endif // USE_OPENAL
