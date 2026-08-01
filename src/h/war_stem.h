//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/h/war_stem.h — Demucs CLI stem extraction into new slots above source
//
// Each extraction job splits a source slot with Demucs (4 stems) and writes
// the requested stem into the next free capture slot above the source pitch
// (same layer), like the split logic. Extracted slots are normal capture
// slots, so they save/load like any other audio.
//-----------------------------------------------------------------------------

#ifndef WAR_STEM_H
#define WAR_STEM_H

#include "war_data.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Drop ownership without free (after move of whole slot to another index).
static inline void _war_slot_null_owned(war_capture_slot* s) {
    if (!s) return;
    s->samples = NULL;
    s->count = 0;
    s->capacity = 0;
}

static inline const char* _war_stem_name(uint8_t mode) {
    switch (mode) {
    case WAR_STEM_VOCALS: return "vocals";
    case WAR_STEM_DRUMS: return "drums";
    case WAR_STEM_BASS: return "bass";
    case WAR_STEM_OTHER: return "other";
    case WAR_STEM_INSTRUMENTAL: return "instrumental";
    default: return "off";
    }
}

static inline int _war_stem_demucs_available(void) {
    if (system("command -v demucs >/dev/null 2>&1") == 0) return 1;
    if (system("python3 -m demucs -h >/dev/null 2>&1") == 0) return 2;
    return 0;
}

static inline int _war_stem_write_wav_f32(const char* path, const float* samples, uint64_t count, int sample_rate) {
    if (!path || !samples || count < 2) return -1;
    uint32_t frames = (uint32_t)(count / 2);
    uint16_t channels = 2;
    uint16_t bits = 16;
    uint32_t byte_rate = (uint32_t)sample_rate * channels * (bits / 8);
    uint16_t block_align = channels * (bits / 8);
    uint32_t data_bytes = frames * block_align;
    uint32_t riff_size = 36 + data_bytes;

    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    fwrite("RIFF", 1, 4, f);
    fwrite(&riff_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    uint32_t fmt_size = 16;
    uint16_t audio_fmt = 1; // PCM
    fwrite(&fmt_size, 4, 1, f);
    fwrite(&audio_fmt, 2, 1, f);
    fwrite(&channels, 2, 1, f);
    uint32_t sr = (uint32_t)sample_rate;
    fwrite(&sr, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data_bytes, 4, 1, f);
    for (uint32_t i = 0; i < frames * 2; i++) {
        float x = samples[i];
        if (x > 1.0f) x = 1.0f;
        if (x < -1.0f) x = -1.0f;
        int16_t s16 = (int16_t)lrintf(x * 32767.0f);
        fwrite(&s16, 2, 1, f);
    }
    fclose(f);
    return 0;
}

static inline float* _war_stem_load_wav_f32(const char* path, uint64_t* out_count, uint64_t target_count) {
    *out_count = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    char riff[4], wave[4];
    uint32_t riff_size = 0;
    if (fread(riff, 1, 4, f) != 4 || memcmp(riff, "RIFF", 4) != 0) { fclose(f); return NULL; }
    fread(&riff_size, 4, 1, f);
    if (fread(wave, 1, 4, f) != 4 || memcmp(wave, "WAVE", 4) != 0) { fclose(f); return NULL; }

    uint16_t audio_fmt = 0, channels = 0, bits = 0;
    uint32_t sample_rate = 0, data_bytes = 0;
    long data_pos = -1;
    while (!feof(f)) {
        char id[4];
        uint32_t sz = 0;
        if (fread(id, 1, 4, f) != 4) break;
        if (fread(&sz, 4, 1, f) != 1) break;
        if (memcmp(id, "fmt ", 4) == 0) {
            fread(&audio_fmt, 2, 1, f);
            fread(&channels, 2, 1, f);
            fread(&sample_rate, 4, 1, f);
            uint32_t skip = 0; fread(&skip, 4, 1, f); // byte rate
            uint16_t ba = 0; fread(&ba, 2, 1, f);
            fread(&bits, 2, 1, f);
            if (sz > 16) fseek(f, (long)(sz - 16), SEEK_CUR);
        } else if (memcmp(id, "data", 4) == 0) {
            data_bytes = sz;
            data_pos = ftell(f);
            fseek(f, (long)sz, SEEK_CUR);
        } else {
            fseek(f, (long)sz, SEEK_CUR);
        }
        if (sz & 1) fseek(f, 1, SEEK_CUR);
    }
    if (data_pos < 0 || channels < 1 || bits == 0) { fclose(f); return NULL; }
    fseek(f, data_pos, SEEK_SET);

    uint32_t bps = bits / 8;
    uint64_t n_samp = data_bytes / bps;
    uint64_t frames = n_samp / channels;
    float* mono_or_st = (float*)malloc(frames * 2 * sizeof(float));
    if (!mono_or_st) { fclose(f); return NULL; }

    for (uint64_t i = 0; i < frames; i++) {
        float L = 0, R = 0;
        for (uint16_t c = 0; c < channels; c++) {
            float v = 0;
            if (bits == 16) {
                int16_t s16 = 0; fread(&s16, 2, 1, f);
                v = (float)s16 / 32768.0f;
            } else if (bits == 24) {
                unsigned char b[3]; fread(b, 1, 3, f);
                int32_t s24 = (int32_t)((b[2] << 16) | (b[1] << 8) | b[0]);
                if (s24 & 0x800000) s24 |= ~0xFFFFFF;
                v = (float)s24 / 8388608.0f;
            } else if (bits == 32 && audio_fmt == 3) {
                float s32; fread(&s32, 4, 1, f); v = s32;
            } else if (bits == 32) {
                int32_t s32 = 0; fread(&s32, 4, 1, f);
                v = (float)s32 / 2147483648.0f;
            } else {
                free(mono_or_st); fclose(f); return NULL;
            }
            if (c == 0) L = v;
            else if (c == 1) R = v;
        }
        if (channels == 1) R = L;
        mono_or_st[i * 2 + 0] = L;
        mono_or_st[i * 2 + 1] = R;
    }
    fclose(f);

    uint64_t src_count = frames * 2;
    if (target_count == 0 || target_count == src_count) {
        *out_count = src_count;
        return mono_or_st;
    }
    // resample/pad/trim to target_count (stereo floats)
    uint64_t dst_frames = target_count / 2;
    uint64_t src_frames = frames;
    float* out = (float*)malloc(target_count * sizeof(float));
    if (!out) { free(mono_or_st); return NULL; }
    for (uint64_t i = 0; i < dst_frames; i++) {
        double sp = (src_frames <= 1) ? 0.0 : (double)i * (double)(src_frames - 1) / (double)(dst_frames - 1);
        uint64_t si = (uint64_t)sp;
        double fr = sp - (double)si;
        if (si >= src_frames - 1) {
            out[i * 2] = mono_or_st[(src_frames - 1) * 2];
            out[i * 2 + 1] = mono_or_st[(src_frames - 1) * 2 + 1];
        } else {
            out[i * 2] = (float)(mono_or_st[si * 2] * (1.0 - fr) + mono_or_st[(si + 1) * 2] * fr);
            out[i * 2 + 1] = (float)(mono_or_st[si * 2 + 1] * (1.0 - fr) + mono_or_st[(si + 1) * 2 + 1] * fr);
        }
    }
    free(mono_or_st);
    *out_count = target_count;
    return out;
}

static inline int _war_stem_find_file(const char* root, const char* name, char* out, size_t out_sz) {
    // demucs layout: root/htdemucs/<track>/name.wav  or root/name.wav
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", root, name);
    if (access(path, R_OK) == 0) { snprintf(out, out_sz, "%s", path); return 0; }

    // search one level: root/*/name.wav
    char cmd[1200];
    snprintf(cmd, sizeof(cmd),
             "find '%s' -maxdepth 3 -type f -name '%s' 2>/dev/null | head -1",
             root, name);
    FILE* p = popen(cmd, "r");
    if (!p) return -1;
    if (!fgets(path, sizeof(path), p)) { pclose(p); return -1; }
    pclose(p);
    size_t n = strlen(path);
    while (n > 0 && (path[n - 1] == '\n' || path[n - 1] == '\r')) path[--n] = 0;
    if (n == 0) return -1;
    snprintf(out, out_sz, "%s", path);
    return 0;
}

static inline int _war_stem_run_demucs(const char* in_wav, const char* out_dir) {
    char cmd[2048];
    int how = _war_stem_demucs_available();
    if (how == 0) return -1;
    // --two-stems not used; full 4-stem htdemucs
    if (how == 1) {
        snprintf(cmd, sizeof(cmd),
                 "demucs -n htdemucs -o '%s' '%s' >/tmp/war_demucs.log 2>&1",
                 out_dir, in_wav);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "python3 -m demucs -n htdemucs -o '%s' '%s' >/tmp/war_demucs.log 2>&1",
                 out_dir, in_wav);
    }
    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "STEM: demucs failed rc=%d (see /tmp/war_demucs.log)\n", rc);
        return -1;
    }
    return 0;
}

// Run the whole extraction for one job: split src slot with demucs and write
// the requested stem into the next free slot above (same layer).
static inline int _war_stem_extract_job(war_env* env, uint32_t src_idx, uint8_t kind) {
    if (!env || src_idx >= 128 * WAR_CAPTURE_SLOT_LAYERS) return -1;
    uint32_t src_pitch = src_idx / WAR_CAPTURE_SLOT_LAYERS;
    uint32_t src_layer = src_idx % WAR_CAPTURE_SLOT_LAYERS + 1;
    war_capture_slot* slot = &env->capture_slots[src_idx];
    if (!slot->samples || slot->count < 2) {
        pthread_mutex_lock(&env->stem_mutex);
        env->stem_last_ok = 0;
        env->stem_last_kind = kind;
        env->stem_last_src = src_idx;
        env->stem_last_dst = UINT32_MAX;
        pthread_mutex_unlock(&env->stem_mutex);
        snprintf(env->status_msg, sizeof(env->status_msg), "stem: empty slot");
        return -1;
    }

    char tmpdir[256], in_wav[300], out_dir[300];
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/war_stem_%d_%u", (int)getpid(), src_idx);
    snprintf(in_wav, sizeof(in_wav), "%s/input.wav", tmpdir);
    snprintf(out_dir, sizeof(out_dir), "%s/out", tmpdir);
    char mk[400];
    snprintf(mk, sizeof(mk), "rm -rf '%s' && mkdir -p '%s' '%s'", tmpdir, tmpdir, out_dir);
    if (system(mk) != 0) {
        snprintf(env->status_msg, sizeof(env->status_msg), "stem: tmpdir failed");
        return -1;
    }

    if (_war_stem_write_wav_f32(in_wav, slot->samples, slot->count, 48000) != 0) {
        snprintf(env->status_msg, sizeof(env->status_msg), "stem: write wav failed");
        return -1;
    }

    snprintf(env->status_msg, sizeof(env->status_msg), "stem: demucs running…");
    if (_war_stem_run_demucs(in_wav, out_dir) != 0) {
        snprintf(env->status_msg, sizeof(env->status_msg), "stem: demucs failed");
        return -1;
    }

    char p_voc[1024], p_dru[1024], p_bas[1024], p_oth[1024];
    if (_war_stem_find_file(out_dir, "vocals.wav", p_voc, sizeof(p_voc)) != 0 ||
        _war_stem_find_file(out_dir, "drums.wav", p_dru, sizeof(p_dru)) != 0 ||
        _war_stem_find_file(out_dir, "bass.wav", p_bas, sizeof(p_bas)) != 0 ||
        _war_stem_find_file(out_dir, "other.wav", p_oth, sizeof(p_oth)) != 0) {
        snprintf(env->status_msg, sizeof(env->status_msg), "stem: output not found");
        return -1;
    }

    uint64_t target = slot->count;
    uint64_t c0 = 0, c1 = 0, c2 = 0, c3 = 0;
    float* v0 = _war_stem_load_wav_f32(p_voc, &c0, target);
    float* v1 = _war_stem_load_wav_f32(p_dru, &c1, target);
    float* v2 = _war_stem_load_wav_f32(p_bas, &c2, target);
    float* v3 = _war_stem_load_wav_f32(p_oth, &c3, target);
    if (!v0 || !v1 || !v2 || !v3) {
        free(v0); free(v1); free(v2); free(v3);
        snprintf(env->status_msg, sizeof(env->status_msg), "stem: load failed");
        return -1;
    }

    float* inst = (float*)malloc(target * sizeof(float));
    if (!inst) {
        free(v0); free(v1); free(v2); free(v3);
        snprintf(env->status_msg, sizeof(env->status_msg), "stem: oom");
        return -1;
    }
    for (uint64_t i = 0; i < target; i++) {
        float s = v1[i] + v2[i] + v3[i];
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        inst[i] = s;
    }

    float* chosen = NULL;
    uint64_t chosen_count = 0;
    switch (kind) {
    case WAR_STEM_VOCALS: chosen = v0; chosen_count = c0; break;
    case WAR_STEM_DRUMS: chosen = v1; chosen_count = c1; break;
    case WAR_STEM_BASS: chosen = v2; chosen_count = c2; break;
    case WAR_STEM_OTHER: chosen = v3; chosen_count = c3; break;
    case WAR_STEM_INSTRUMENTAL: chosen = inst; chosen_count = target; break;
    default: chosen = NULL; break;
    }
    if (!chosen || chosen_count < 2) {
        free(v0); free(v1); free(v2); free(v3); free(inst);
        snprintf(env->status_msg, sizeof(env->status_msg), "stem: bad kind");
        return -1;
    }

    // find next free slot above (same layer, like split)
    pthread_mutex_lock(&env->stem_mutex);
    uint32_t dst_idx = UINT32_MAX;
    for (uint32_t p = src_pitch + 1; p < 128; p++) {
        uint32_t mi = p * WAR_CAPTURE_SLOT_LAYERS + (src_layer - 1);
        if (!env->capture_slots[mi].samples || env->capture_slots[mi].count < 2) {
            dst_idx = mi;
            break;
        }
    }
    if (dst_idx == UINT32_MAX) {
        env->stem_last_ok = 0;
        env->stem_last_kind = kind;
        env->stem_last_src = src_idx;
        env->stem_last_dst = UINT32_MAX;
        pthread_mutex_unlock(&env->stem_mutex);
        free(v0); free(v1); free(v2); free(v3); free(inst);
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "stem: no free slot above pitch %u", src_pitch);
        return -1;
    }

    // install into dest: params copied from source; samples written before
    // count so concurrent readers see an empty slot, never a dangling one
    war_capture_slot* dst = &env->capture_slots[dst_idx];
    free(dst->samples);
    dst->samples = NULL;
    dst->count = 0;
    dst->capacity = 0;
    dst->gain = slot->gain;
    dst->pan = slot->pan;
    dst->eq1 = slot->eq1;
    dst->eq2 = slot->eq2;
    dst->attack = slot->attack;
    dst->sustain = slot->sustain;
    dst->release = slot->release;
    dst->effect_flags = slot->effect_flags;
    memcpy(dst->effect_params, slot->effect_params,
           sizeof(double) * WAR_EFFECT_COUNT * WAR_EFFECT_PARAMS);
    dst->samples = chosen;
    dst->count = chosen_count;
    dst->capacity = chosen_count;
    env->stem_last_ok = 1;
    env->stem_last_kind = kind;
    env->stem_last_src = src_idx;
    env->stem_last_dst = dst_idx;
    pthread_mutex_unlock(&env->stem_mutex);

    if (chosen != v0) free(v0);
    if (chosen != v1) free(v1);
    if (chosen != v2) free(v2);
    if (chosen != v3) free(v3);
    if (chosen != inst) free(inst);

    char rm[300];
    snprintf(rm, sizeof(rm), "rm -rf '%s'", tmpdir);
    system(rm);
    snprintf(env->status_msg, sizeof(env->status_msg),
             "stem: %s -> pitch %u", _war_stem_name(kind), dst_idx / WAR_CAPTURE_SLOT_LAYERS);
    return 0;
}

static void* _war_stem_worker(void* arg) {
    war_env* env = (war_env*)arg;
    for (;;) {
        war_stem_job job;
        job.src_idx = UINT32_MAX;
        job.kind = WAR_STEM_OFF;
        uint32_t done = 0, total = 0;
        pthread_mutex_lock(&env->stem_mutex);
        if (env->stem_cancel) {
            env->stem_queue_len = 0;
            env->stem_worker_busy = 0;
            env->stem_thread_alive = 0;
            env->stem_cancel = 0;
            pthread_mutex_unlock(&env->stem_mutex);
            snprintf(env->status_msg, sizeof(env->status_msg), "stem: cancelled");
            break;
        }
        if (env->stem_queue_len == 0) {
            env->stem_worker_busy = 0;
            env->stem_thread_alive = 0;
            pthread_mutex_unlock(&env->stem_mutex);
            break;
        }
        job = env->stem_queue[0];
        for (uint32_t i = 1; i < env->stem_queue_len; i++)
            env->stem_queue[i - 1] = env->stem_queue[i];
        env->stem_queue_len--;
        env->stem_worker_busy = 1;
        done = env->stem_done_count;
        total = env->stem_total_count;
        pthread_mutex_unlock(&env->stem_mutex);

        snprintf(env->status_msg, sizeof(env->status_msg),
                 "stem: %s %u/%u…", _war_stem_name(job.kind), done + 1, total);
        int rc = _war_stem_extract_job(env, job.src_idx, job.kind);

        pthread_mutex_lock(&env->stem_mutex);
        env->stem_done_count++;
        done = env->stem_done_count;
        total = env->stem_total_count;
        int more = env->stem_queue_len > 0 && !env->stem_cancel;
        pthread_mutex_unlock(&env->stem_mutex);

        if (rc == 0 && !more)
            snprintf(env->status_msg, sizeof(env->status_msg),
                     "stem: ready (%u/%u)", done, total);
        if (!more) {
            pthread_mutex_lock(&env->stem_mutex);
            env->stem_worker_busy = 0;
            env->stem_thread_alive = 0;
            pthread_mutex_unlock(&env->stem_mutex);
            break;
        }
    }
    return NULL;
}

static inline void _war_stem_ensure_thread(war_env* env) {
    if (!env) return;
    pthread_mutex_lock(&env->stem_mutex);
    if (env->stem_thread_alive) {
        pthread_mutex_unlock(&env->stem_mutex);
        return;
    }
    env->stem_thread_alive = 1;
    env->stem_worker_busy = 1;
    pthread_mutex_unlock(&env->stem_mutex);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&env->stem_thread, &attr, _war_stem_worker, env) != 0) {
        pthread_mutex_lock(&env->stem_mutex);
        env->stem_thread_alive = 0;
        env->stem_worker_busy = 0;
        pthread_mutex_unlock(&env->stem_mutex);
        snprintf(env->status_msg, sizeof(env->status_msg), "stem: thread failed");
    }
    pthread_attr_destroy(&attr);
}

static inline void war_stem_init(war_env* env) {
    if (!env) return;
    pthread_mutex_init(&env->stem_mutex, NULL);
    env->stem_thread_alive = 0;
    env->stem_worker_busy = 0;
    env->stem_cancel = 0;
    env->stem_queue_len = 0;
    env->stem_done_count = 0;
    env->stem_total_count = 0;
    env->stem_last_ok = 0;
    env->stem_last_kind = WAR_STEM_OFF;
    env->stem_last_src = 0;
    env->stem_last_dst = UINT32_MAX;
}

static inline void war_stem_shutdown(war_env* env) {
    if (!env) return;
    pthread_mutex_lock(&env->stem_mutex);
    env->stem_cancel = 1;
    env->stem_queue_len = 0;
    pthread_mutex_unlock(&env->stem_mutex);
    // detached worker exits on cancel/empty; brief wait
    for (int i = 0; i < 50; i++) {
        pthread_mutex_lock(&env->stem_mutex);
        int alive = env->stem_thread_alive;
        pthread_mutex_unlock(&env->stem_mutex);
        if (!alive) break;
        usleep(100000);
    }
    pthread_mutex_destroy(&env->stem_mutex);
}

static inline void war_stem_enqueue(war_env* env, uint32_t src_idx, uint8_t kind) {
    if (!env || src_idx >= 128 * WAR_CAPTURE_SLOT_LAYERS) return;
    if (kind < WAR_STEM_VOCALS || kind > WAR_STEM_INSTRUMENTAL) return;
    pthread_mutex_lock(&env->stem_mutex);
    for (uint32_t i = 0; i < env->stem_queue_len; i++) {
        if (env->stem_queue[i].src_idx == src_idx && env->stem_queue[i].kind == kind) {
            pthread_mutex_unlock(&env->stem_mutex);
            return;
        }
    }
    if (env->stem_queue_len >= WAR_STEM_QUEUE_MAX) {
        pthread_mutex_unlock(&env->stem_mutex);
        snprintf(env->status_msg, sizeof(env->status_msg), "stem: queue full");
        return;
    }
    int start_counts = (!env->stem_worker_busy && env->stem_queue_len == 0);
    env->stem_queue[env->stem_queue_len].src_idx = src_idx;
    env->stem_queue[env->stem_queue_len].kind = kind;
    env->stem_queue_len++;
    if (start_counts) {
        env->stem_done_count = 0;
        env->stem_total_count = 1;
    } else {
        env->stem_total_count++;
    }
    pthread_mutex_unlock(&env->stem_mutex);
    _war_stem_ensure_thread(env);
}

// Selected pitch rows (visual anchor→cursor, else cursor row), same as
// _war_sel_pitches (kept local to this header to avoid include order issues).
static inline int _war_stem_sel_pitches(war_env* env, uint32_t* out) {
    if (!env || !env->ctx_cursor || !env->ctx_wayland) return 0;
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur->instance_count) return 0;
    double gr = (double)env->ctx_wayland->gutter_rows;
    int np = 0;
    if (cur->visual_active) {
        float y0 = cur->visual_anchor_row;
        float y1 = cur->instance[0].pos[1];
        if (y0 > y1) { float t = y0; y0 = y1; y1 = t; }
        int p0 = (int)(y0 - gr + 0.5);
        int p1 = (int)(y1 - gr + 0.5);
        if (p0 < 0) p0 = 0;
        if (p1 > 127) p1 = 127;
        for (int p = p0; p <= p1; p++) out[np++] = (uint32_t)p;
    } else {
        int p = (int)(cur->instance[0].pos[1] - gr + 0.5);
        if (p < 0) p = 0;
        if (p > 127) p = 127;
        out[np++] = (uint32_t)p;
    }
    return np;
}

static inline void war_stem_enqueue_selection(war_env* env, uint8_t kind) {
    if (!env || !env->ctx_cursor || !env->ctx_wayland) return;
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur->instance_count) return;
    if (_war_stem_demucs_available() == 0) {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "stem: demucs not found (pip install demucs)");
        return;
    }
    uint32_t pitches[128];
    int np = _war_stem_sel_pitches(env, pitches);
    uint32_t layer = cur->layer;
    if (layer < 1 || layer > 9) layer = 1;
    int queued = 0;
    for (int i = 0; i < np; i++) {
        uint32_t idx = pitches[i] * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
        if (env->capture_slots[idx].samples && env->capture_slots[idx].count >= 2) {
            war_stem_enqueue(env, idx, kind);
            queued++;
        }
    }
    if (queued == 0)
        snprintf(env->status_msg, sizeof(env->status_msg), "stem: no audio in selection");
    else
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "stem: queued %s for %d row(s)", _war_stem_name(kind), queued);
}

static inline void war_stem_enqueue_selection_all(war_env* env) {
    war_stem_enqueue_selection(env, WAR_STEM_VOCALS);
    war_stem_enqueue_selection(env, WAR_STEM_DRUMS);
    war_stem_enqueue_selection(env, WAR_STEM_BASS);
    war_stem_enqueue_selection(env, WAR_STEM_OTHER);
    war_stem_enqueue_selection(env, WAR_STEM_INSTRUMENTAL);
    snprintf(env->status_msg, sizeof(env->status_msg),
             "stem: queued all 5 stems for selection");
}

static inline void war_stem_cancel(war_env* env) {
    if (!env) return;
    pthread_mutex_lock(&env->stem_mutex);
    env->stem_cancel = 1;
    env->stem_queue_len = 0;
    pthread_mutex_unlock(&env->stem_mutex);
    snprintf(env->status_msg, sizeof(env->status_msg), "stem: cancelled");
}

static inline void war_stem_status(war_env* env) {
    if (!env) return;
    int dem = _war_stem_demucs_available();
    pthread_mutex_lock(&env->stem_mutex);
    uint32_t qlen = env->stem_queue_len;
    uint32_t done = env->stem_done_count;
    uint32_t total = env->stem_total_count;
    uint8_t lk = env->stem_last_ok;
    uint8_t lkind = env->stem_last_kind;
    uint32_t lsrc = env->stem_last_src;
    uint32_t ldst = env->stem_last_dst;
    pthread_mutex_unlock(&env->stem_mutex);
    if (lkind != WAR_STEM_OFF) {
        uint32_t lsrc_p = lsrc / WAR_CAPTURE_SLOT_LAYERS;
        uint32_t lsrc_l = lsrc % WAR_CAPTURE_SLOT_LAYERS + 1;
        if (lk)
            snprintf(env->status_msg, sizeof(env->status_msg),
                     "stem: %s -> pitch %u (src p%u/l%u) done=%u/%u queued=%u demucs=%s",
                     _war_stem_name(lkind), ldst / WAR_CAPTURE_SLOT_LAYERS,
                     lsrc_p, lsrc_l, done, total, qlen, dem ? "yes" : "no");
        else
            snprintf(env->status_msg, sizeof(env->status_msg),
                     "stem: last %s failed (src p%u/l%u) done=%u/%u queued=%u demucs=%s",
                     _war_stem_name(lkind), lsrc_p, lsrc_l, done, total, qlen,
                     dem ? "yes" : "no");
    } else {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "stem: idle done=%u/%u queued=%u demucs=%s",
                 done, total, qlen, dem ? "yes" : "no");
    }
}

// Command entry: parse rest after ":stem" or handle ":stemvocals"
static inline void war_stem_cmd(war_env* env) {
    if (!env || !env->cmd_active) return;
    const char* buf = env->cmd_buf;
    int len = (int)env->cmd_len;
    // :stemvocals on|off (alias)
    if (len >= 11 && strncmp(buf, ":stemvocals", 11) == 0) {
        const char* rest = buf + 11;
        while (*rest == ' ' || *rest == '\t') rest++;
        if (strcmp(rest, "on") == 0 || strcmp(rest, "vocals") == 0 || *rest == '\0')
            war_stem_enqueue_selection(env, WAR_STEM_VOCALS);
        else if (strcmp(rest, "off") == 0)
            war_stem_cancel(env);
        else
            snprintf(env->status_msg, sizeof(env->status_msg),
                     "usage: :stemvocals on|off");
        return;
    }
    if (len < 5 || strncmp(buf, ":stem", 5) != 0) return;
    const char* rest = buf + 5;
    while (*rest == ' ' || *rest == '\t') rest++;
    if (*rest == '\0' || strcmp(rest, "status") == 0) {
        war_stem_status(env);
        return;
    }
    if (strcmp(rest, "extract") == 0) {
        war_stem_enqueue_selection_all(env);
        return;
    }
    if (strcmp(rest, "clear") == 0 || strcmp(rest, "cancel") == 0) {
        war_stem_cancel(env);
        return;
    }
    if (strcmp(rest, "vocals") == 0 || strcmp(rest, "vocal") == 0) {
        war_stem_enqueue_selection(env, WAR_STEM_VOCALS);
        return;
    }
    if (strcmp(rest, "drums") == 0 || strcmp(rest, "drum") == 0) {
        war_stem_enqueue_selection(env, WAR_STEM_DRUMS);
        return;
    }
    if (strcmp(rest, "bass") == 0) {
        war_stem_enqueue_selection(env, WAR_STEM_BASS);
        return;
    }
    if (strcmp(rest, "other") == 0) {
        war_stem_enqueue_selection(env, WAR_STEM_OTHER);
        return;
    }
    if (strcmp(rest, "instrumental") == 0 || strcmp(rest, "inst") == 0) {
        war_stem_enqueue_selection(env, WAR_STEM_INSTRUMENTAL);
        return;
    }
    snprintf(env->status_msg, sizeof(env->status_msg),
             "usage: :stem extract|vocals|drums|bass|other|instrumental|status|clear");
}

#endif // WAR_STEM_H
