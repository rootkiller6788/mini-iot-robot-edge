#include "wake_word.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MelFilterbank* mel_filterbank_create(int32_t num_filters, int32_t fft_size,
    int32_t sample_rate, float low_freq, float high_freq)
{
    MelFilterbank* fb = (MelFilterbank*)calloc(1, sizeof(MelFilterbank));
    if (!fb) return NULL;
    fb->num_filters = num_filters;
    fb->fft_size = fft_size;
    fb->sample_rate = sample_rate;
    fb->low_freq = low_freq;
    fb->high_freq = high_freq;

    float low_mel = mel_hz_to_mel(low_freq);
    float high_mel = mel_hz_to_mel(high_freq);
    float mel_step = (high_mel - low_mel) / (float)(num_filters + 1);
    for (int32_t i = 0; i < num_filters; i++) {
        float center_mel = low_mel + mel_step * (float)(i + 1);
        float center_hz = mel_mel_to_hz(center_mel);
        for (int32_t j = 0; j < fft_size; j++) {
            float freq = (float)j * (float)sample_rate / (float)fft_size;
            float diff = freq - center_hz;
            float bw = 200.0f;
            fb->filterbank_coeffs[i][j] = expf(-diff * diff / (2.0f * bw * bw));
        }
    }
    fb->is_initialized = true;
    return fb;
}

void mel_filterbank_free(MelFilterbank* filterbank)
{
    free(filterbank);
}

void mel_filterbank_apply(const MelFilterbank* filterbank,
    const float* power_spectrum, float* mel_energies)
{
    if (!filterbank || !power_spectrum || !mel_energies) return;
    for (int32_t i = 0; i < filterbank->num_filters; i++) {
        mel_energies[i] = 0.0f;
        for (int32_t j = 0; j < filterbank->fft_size; j++) {
            mel_energies[i] += filterbank->filterbank_coeffs[i][j] * power_spectrum[j];
        }
        if (mel_energies[i] < 1e-10f) mel_energies[i] = 1e-10f;
        mel_energies[i] = logf(mel_energies[i]);
    }
}

float mel_hz_to_mel(float hz)
{
    if (hz <= 0) return 0;
    return 2595.0f * log10f(1.0f + hz / 700.0f);
}

float mel_mel_to_hz(float mel)
{
    return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
}

MFCCExtractor* mfcc_extractor_create(int32_t sample_rate, int32_t frame_length_ms,
    int32_t frame_step_ms, int32_t num_coeffs, int32_t num_mel_bins)
{
    MFCCExtractor* extractor = (MFCCExtractor*)calloc(1, sizeof(MFCCExtractor));
    if (!extractor) return NULL;
    extractor->frame_length_samples = (int32_t)((float)sample_rate * (float)frame_length_ms / 1000.0f);
    extractor->frame_step_samples   = (int32_t)((float)sample_rate * (float)frame_step_ms / 1000.0f);
    extractor->num_coefficients = num_coeffs;
    extractor->num_mel_bins = num_mel_bins;
    extractor->pre_emphasis_coeff = 0.97f;
    extractor->window_type = WAKE_WORD_WINDOW_HANNING;

    extractor->filterbank = mel_filterbank_create(num_mel_bins, WAKE_WORD_FILTERBANK_NUM_FFT,
        sample_rate, 20.0f, (float)sample_rate / 2.0f);
    if (!extractor->filterbank) { free(extractor); return NULL; }

    if (extractor->frame_length_samples > WAKE_WORD_MAX_FRAME_SAMPLES) {
        extractor->frame_length_samples = WAKE_WORD_MAX_FRAME_SAMPLES;
    }
    for (int32_t i = 0; i < extractor->frame_length_samples; i++) {
        float t = (float)i / (float)(extractor->frame_length_samples - 1);
        switch (extractor->window_type) {
        case WAKE_WORD_WINDOW_HANNING:
            extractor->window_coeffs[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * t));
            break;
        case WAKE_WORD_WINDOW_HAMMING:
            extractor->window_coeffs[i] = 0.54f - 0.46f * cosf(2.0f * (float)M_PI * t);
            break;
        default:
            extractor->window_coeffs[i] = 1.0f;
            break;
        }
    }
    extractor->is_initialized = true;
    return extractor;
}

void mfcc_extractor_free(MFCCExtractor* extractor)
{
    if (!extractor) return;
    mel_filterbank_free(extractor->filterbank);
    free(extractor);
}

static void mfcc_apply_dct(const float* log_mel, int32_t num_mel, float* mfcc, int32_t num_coeffs)
{
    for (int32_t i = 0; i < num_coeffs; i++) {
        mfcc[i] = 0.0f;
        for (int32_t j = 0; j < num_mel; j++) {
            mfcc[i] += log_mel[j] * cosf((float)M_PI * (float)i * ((float)j + 0.5f) / (float)num_mel);
        }
    }
}

WakeWordStatus mfcc_extractor_process_frame(MFCCExtractor* extractor,
    const int16_t* samples, int32_t num_samples, MFCCFrame* out_frame)
{
    if (!extractor || !samples || !out_frame) return WAKE_WORD_STATUS_INVALID_PARAM;
    if (!extractor->is_initialized) return WAKE_WORD_STATUS_NOT_INITIALIZED;

    int32_t frame_len = extractor->frame_length_samples;
    if (num_samples < frame_len) frame_len = num_samples;

    float pre_emphasized[WAKE_WORD_MAX_FRAME_SAMPLES];
    pre_emphasized[0] = (float)samples[0] / 32768.0f;
    for (int32_t i = 1; i < frame_len; i++) {
        pre_emphasized[i] = ((float)samples[i] / 32768.0f)
            - extractor->pre_emphasis_coeff * ((float)samples[i - 1] / 32768.0f);
    }

    float windowed[WAKE_WORD_MAX_FRAME_SAMPLES];
    float energy = 0.0f;
    for (int32_t i = 0; i < frame_len; i++) {
        windowed[i] = pre_emphasized[i] * extractor->window_coeffs[i];
        energy += windowed[i] * windowed[i];
    }

    float power_spectrum[WAKE_WORD_FILTERBANK_NUM_FFT];
    for (int32_t k = 0; k < WAKE_WORD_FILTERBANK_NUM_FFT; k++) {
        float real = 0.0f, imag = 0.0f;
        for (int32_t n = 0; n < frame_len; n++) {
            float angle = -2.0f * (float)M_PI * (float)k * (float)n / (float)WAKE_WORD_FILTERBANK_NUM_FFT;
            real += windowed[n] * cosf(angle);
            imag += windowed[n] * sinf(angle);
        }
        power_spectrum[k] = (real * real + imag * imag) / (float)frame_len;
        if (power_spectrum[k] < 1e-10f) power_spectrum[k] = 1e-10f;
    }

    float mel_energies[WAKE_WORD_MFCC_NUM_MEL_BINS];
    mel_filterbank_apply(extractor->filterbank, power_spectrum, mel_energies);

    mfcc_apply_dct(mel_energies, extractor->num_mel_bins,
        out_frame->coefficients, extractor->num_coefficients);

    out_frame->energy = energy;
    out_frame->is_speech = (energy > 0.001f);
    extractor->history[extractor->history_count % WAKE_WORD_MAX_HISTORY_FRAMES] = *out_frame;
    extractor->history_count++;

    return WAKE_WORD_STATUS_OK;
}

void mfcc_extractor_reset(MFCCExtractor* extractor)
{
    if (!extractor) return;
    extractor->history_count = 0;
    memset(extractor->history, 0, sizeof(extractor->history));
}

WakeWordModel* wake_word_model_create(void)
{
    WakeWordModel* model = (WakeWordModel*)calloc(1, sizeof(WakeWordModel));
    if (!model) return NULL;
    model->probability_threshold = WAKE_WORD_PROB_THRESHOLD;
    model->detection_smoothing = 0.1f;
    return model;
}

void wake_word_model_free(WakeWordModel* model)
{
    if (!model) return;
    free(model->model_data);
    free(model);
}

WakeWordStatus wake_word_model_load(WakeWordModel* model, const uint8_t* model_bytes,
    size_t model_bytes_len, WakeWordModelType type)
{
    if (!model || !model_bytes || model_bytes_len == 0) return WAKE_WORD_STATUS_INVALID_PARAM;
    model->model_type = type;
    model->model_size = model_bytes_len;
    model->model_data = malloc(model_bytes_len);
    if (!model->model_data) return WAKE_WORD_STATUS_MEMORY;
    memcpy(model->model_data, model_bytes, model_bytes_len);
    model->is_loaded = true;
    model->num_keywords = 1;
    strncpy(model->keyword_labels[0], "hey_device", 31);
    return WAKE_WORD_STATUS_OK;
}

WakeWordStatus wake_word_model_predict(const WakeWordModel* model,
    const float* features, int32_t feature_count, float* probabilities, int32_t num_classes)
{
    if (!model || !features || !probabilities) return WAKE_WORD_STATUS_INVALID_PARAM;
    if (!model->is_loaded) return WAKE_WORD_STATUS_NOT_INITIALIZED;

    float max_val = features[0];
    for (int32_t i = 1; i < feature_count; i++) {
        if (features[i] > max_val) max_val = features[i];
    }
    for (int32_t c = 0; c < num_classes; c++) {
        float score = 0.0f;
        for (int32_t i = 0; i < feature_count; i++) {
            score += features[i] * (0.1f * (float)((i + c * 7) % 13));
        }
        probabilities[c] = 1.0f / (1.0f + expf(-score));
    }
    float sum = 0.0f;
    for (int32_t c = 0; c < num_classes; c++) sum += probabilities[c];
    if (sum > 0.0f) {
        for (int32_t c = 0; c < num_classes; c++) probabilities[c] /= sum;
    }
    return WAKE_WORD_STATUS_OK;
}

WakeWordStatus wake_word_model_set_threshold(WakeWordModel* model, float threshold)
{
    if (!model) return WAKE_WORD_STATUS_INVALID_PARAM;
    model->probability_threshold = threshold;
    return WAKE_WORD_STATUS_OK;
}

SlidingWindow* sliding_window_create(int32_t window_size_ms, int32_t frame_step_ms)
{
    SlidingWindow* win = (SlidingWindow*)calloc(1, sizeof(SlidingWindow));
    if (!win) return NULL;
    win->window_size = window_size_ms / frame_step_ms;
    if (win->window_size < 1) win->window_size = 1;
    if (win->window_size > WAKE_WORD_MAX_HISTORY_FRAMES)
        win->window_size = WAKE_WORD_MAX_HISTORY_FRAMES;
    win->current_index = 0;
    return win;
}

void sliding_window_free(SlidingWindow* window)
{
    free(window);
}

WakeWordStatus sliding_window_push(SlidingWindow* window, const MFCCFrame* frame)
{
    if (!window || !frame) return WAKE_WORD_STATUS_INVALID_PARAM;
    window->frames[window->current_index % WAKE_WORD_MAX_HISTORY_FRAMES] = *frame;
    window->current_index++;
    return WAKE_WORD_STATUS_OK;
}

WakeWordStatus sliding_window_get_features(const SlidingWindow* window, float* features, int32_t max_features)
{
    if (!window || !features) return WAKE_WORD_STATUS_INVALID_PARAM;
    int32_t idx = 0;
    int32_t start = window->current_index - window->window_size;
    if (start < 0) start = 0;
    for (int32_t i = start; i < window->current_index && idx < max_features; i++) {
        for (int32_t c = 0; c < WAKE_WORD_MFCC_NUM_COEFFS && idx < max_features; c++) {
            features[idx++] = window->frames[i % WAKE_WORD_MAX_HISTORY_FRAMES].coefficients[c];
        }
    }
    return WAKE_WORD_STATUS_OK;
}

int32_t sliding_window_count(const SlidingWindow* window)
{
    if (!window) return 0;
    int32_t cnt = window->current_index;
    return cnt < window->window_size ? cnt : window->window_size;
}

bool sliding_window_is_full(const SlidingWindow* window)
{
    if (!window) return false;
    return window->current_index >= window->window_size;
}

void sliding_window_reset(SlidingWindow* window)
{
    if (!window) return;
    window->current_index = 0;
    memset(window->frames, 0, sizeof(window->frames));
    memset(window->probabilities, 0, sizeof(window->probabilities));
    window->max_probability_value = 0.0f;
}

PostProcessor* postprocessor_create(int32_t min_activation_ms, int32_t cooldown_ms)
{
    PostProcessor* pp = (PostProcessor*)calloc(1, sizeof(PostProcessor));
    if (!pp) return NULL;
    pp->min_activation_ms = min_activation_ms;
    pp->cooldown_ms = cooldown_ms;
    return pp;
}

void postprocessor_free(PostProcessor* postproc)
{
    free(postproc);
}

WakeWordStatus postprocessor_process(PostProcessor* postproc,
    const float* probabilities, int32_t num_keywords, int32_t timestamp_ms)
{
    if (!postproc || !probabilities) return WAKE_WORD_STATUS_INVALID_PARAM;
    float max_p = probabilities[0];
    int32_t max_i = 0;
    for (int32_t i = 1; i < num_keywords; i++) {
        if (probabilities[i] > max_p) { max_p = probabilities[i]; max_i = i; }
    }
    if (max_p >= WAKE_WORD_PROB_THRESHOLD) {
        postproc->activation_count++;
        if (postproc->detection_count < 16) {
            WakeWordDetection* det = &postproc->recent_detections[postproc->detection_count];
            strncpy(det->keyword, (max_i == 0) ? "hey_device" : "unknown", WAKE_WORD_MAX_PHRASE_LEN - 1);
            det->probability = max_p;
            det->timestamp_ms = timestamp_ms;
            det->is_valid = true;
            postproc->detection_count++;
        }
        postproc->last_activation_time = timestamp_ms;
    } else {
        postproc->deactivation_count++;
    }
    return WAKE_WORD_STATUS_OK;
}

bool postprocessor_is_detected(const PostProcessor* postproc, char* out_keyword, int32_t max_len)
{
    if (!postproc) return false;
    if (postproc->detection_count > 0) {
        const WakeWordDetection* d = &postproc->recent_detections[postproc->detection_count - 1];
        if (d->is_valid && out_keyword) {
            strncpy(out_keyword, d->keyword, (size_t)max_len - 1);
            out_keyword[max_len - 1] = '\0';
        }
        return d->is_valid;
    }
    return false;
}

bool postprocessor_ignore_short_activation(PostProcessor* postproc, int32_t current_time_ms)
{
    if (!postproc) return false;
    int32_t since_last = current_time_ms - postproc->last_activation_time;
    if (since_last > postproc->min_activation_ms) {
        postproc->activation_count = 0;
        postproc->deactivation_count = 0;
        return true;
    }
    return since_last < postproc->min_activation_ms;
}

void postprocessor_reset(PostProcessor* postproc)
{
    if (!postproc) return;
    postproc->detection_count = 0;
    postproc->activation_count = 0;
    postproc->deactivation_count = 0;
    postproc->last_activation_time = 0;
    memset(postproc->recent_detections, 0, sizeof(postproc->recent_detections));
}

WakeWordDetector* wake_word_detector_create(int32_t sample_rate, const char* keyword)
{
    WakeWordDetector* det = (WakeWordDetector*)calloc(1, sizeof(WakeWordDetector));
    if (!det) return NULL;
    det->sample_rate = sample_rate;
    det->probability_threshold = WAKE_WORD_PROB_THRESHOLD;
    det->state = WAKE_WORD_STATE_IDLE;
    if (keyword) {
        strncpy(det->target_keyword, keyword, WAKE_WORD_MAX_PHRASE_LEN - 1);
    } else {
        strncpy(det->target_keyword, "hey_device", WAKE_WORD_MAX_PHRASE_LEN - 1);
    }
    det->tensor_arena_size = TFLITE_MICRO_DEFAULT_ARENA_SIZE;
    return det;
}

void wake_word_detector_free(WakeWordDetector* detector)
{
    if (!detector) return;
    mfcc_extractor_free(detector->mfcc);
    wake_word_model_free(detector->model);
    sliding_window_free(detector->window);
    postprocessor_free(detector->postproc);
    free(detector->tensor_arena);
    free(detector);
}

WakeWordStatus wake_word_detector_init(WakeWordDetector* detector)
{
    if (!detector) return WAKE_WORD_STATUS_INVALID_PARAM;
    detector->mfcc = mfcc_extractor_create(detector->sample_rate,
        WAKE_WORD_MFCC_FRAME_LEN_MS, WAKE_WORD_MFCC_FRAME_STEP_MS,
        WAKE_WORD_MFCC_NUM_COEFFS, WAKE_WORD_MFCC_NUM_MEL_BINS);
    if (!detector->mfcc) return WAKE_WORD_STATUS_MEMORY;
    detector->model = wake_word_model_create();
    if (!detector->model) return WAKE_WORD_STATUS_MEMORY;
    detector->window = sliding_window_create(WAKE_WORD_SLIDING_WINDOW_MS, WAKE_WORD_MFCC_FRAME_STEP_MS);
    if (!detector->window) return WAKE_WORD_STATUS_MEMORY;
    detector->postproc = postprocessor_create(WAKE_WORD_MIN_ACTIVATION_MS,
        WAKE_WORD_DETECTION_COOLDOWN_MS);
    if (!detector->postproc) return WAKE_WORD_STATUS_MEMORY;
    detector->tensor_arena = malloc(detector->tensor_arena_size);
    if (!detector->tensor_arena) return WAKE_WORD_STATUS_MEMORY;
    detector->is_initialized = true;
    detector->state = WAKE_WORD_STATE_LISTENING;
    return WAKE_WORD_STATUS_OK;
}

WakeWordStatus wake_word_detector_load_model(WakeWordDetector* detector,
    const uint8_t* model_data, size_t model_size, WakeWordModelType type)
{
    if (!detector || !detector->model) return WAKE_WORD_STATUS_NOT_INITIALIZED;
    return wake_word_model_load(detector->model, model_data, model_size, type);
}

WakeWordStatus wake_word_detector_process_samples(WakeWordDetector* detector,
    const int16_t* samples, int32_t num_samples)
{
    if (!detector || !samples) return WAKE_WORD_STATUS_INVALID_PARAM;
    if (!detector->is_initialized) return WAKE_WORD_STATUS_NOT_INITIALIZED;

    if (detector->state != WAKE_WORD_STATE_LISTENING) {
        detector->state = WAKE_WORD_STATE_LISTENING;
    }

    int32_t frame_step = detector->mfcc->frame_step_samples;
    int32_t pos = 0;
    while (pos + detector->mfcc->frame_length_samples <= num_samples) {
        MFCCFrame frame;
        WakeWordStatus s = mfcc_extractor_process_frame(detector->mfcc,
            samples + pos, num_samples - pos, &frame);
        if (s != WAKE_WORD_STATUS_OK) return s;
        sliding_window_push(detector->window, &frame);

        if (sliding_window_is_full(detector->window)) {
            float features[WAKE_WORD_MAX_HISTORY_FRAMES * WAKE_WORD_MFCC_NUM_COEFFS];
            sliding_window_get_features(detector->window, features,
                WAKE_WORD_MAX_HISTORY_FRAMES * WAKE_WORD_MFCC_NUM_COEFFS);

            float probs[WAKE_WORD_MAX_KEYWORDS] = {0};
            int32_t feat_cnt = sliding_window_count(detector->window) * WAKE_WORD_MFCC_NUM_COEFFS;
            wake_word_model_predict(detector->model, features, feat_cnt, probs, 2);

            detector->current_time_ms += frame_step * 1000 / detector->sample_rate;
            postprocessor_process(detector->postproc, probs, 2, detector->current_time_ms);

            for (int32_t i = 0; i < 2; i++) detector->window->probabilities[i] = probs[i];
            float max_p = probs[0] > probs[1] ? probs[0] : probs[1];
            detector->window->max_probability_value = max_p;

            if (max_p >= detector->probability_threshold) {
                detector->state = WAKE_WORD_STATE_DETECTED;
                if (!postprocessor_ignore_short_activation(detector->postproc,
                    detector->current_time_ms)) {
                    detector->state = WAKE_WORD_STATE_ACTIVATED;
                }
            }
        }
        pos += frame_step;
    }
    return WAKE_WORD_STATUS_OK;
}

bool wake_word_detector_is_awake(const WakeWordDetector* detector)
{
    if (!detector) return false;
    return detector->state == WAKE_WORD_STATE_ACTIVATED;
}

WakeWordDetection wake_word_detector_get_detection(const WakeWordDetector* detector)
{
    WakeWordDetection det = {0};
    if (!detector || !detector->postproc) return det;
    if (detector->postproc->detection_count > 0) {
        return detector->postproc->recent_detections[detector->postproc->detection_count - 1];
    }
    return det;
}

WakeWordState wake_word_detector_get_state(const WakeWordDetector* detector)
{
    if (!detector) return WAKE_WORD_STATE_IDLE;
    return detector->state;
}

void wake_word_detector_set_threshold(WakeWordDetector* detector, float threshold)
{
    if (!detector) return;
    detector->probability_threshold = threshold;
    if (detector->model) detector->model->probability_threshold = threshold;
}

void wake_word_detector_reset(WakeWordDetector* detector)
{
    if (!detector) return;
    mfcc_extractor_reset(detector->mfcc);
    sliding_window_reset(detector->window);
    postprocessor_reset(detector->postproc);
    detector->state = WAKE_WORD_STATE_IDLE;
    detector->current_time_ms = 0;
}
