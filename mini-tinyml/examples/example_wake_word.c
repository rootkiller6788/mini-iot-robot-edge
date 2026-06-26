#include "tflite_micro.h"
#include "wake_word.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define EXAMPLE_SAMPLE_RATE  16000
#define EXAMPLE_DURATION_MS  2000
#define EXAMPLE_NUM_SAMPLES  ((EXAMPLE_SAMPLE_RATE * EXAMPLE_DURATION_MS) / 1000)

static void generate_sine_wave(int16_t* buffer, int32_t num_samples, float freq, int32_t sample_rate)
{
    for (int32_t i = 0; i < num_samples; i++) {
        float t = (float)i / (float)sample_rate;
        buffer[i] = (int16_t)(16000.0f * sinf(2.0f * 3.14159265358979323846f * freq * t));
    }
}

static void generate_white_noise(int16_t* buffer, int32_t num_samples)
{
    for (int32_t i = 0; i < num_samples; i++) {
        buffer[i] = (int16_t)((rand() % 16000) - 8000);
    }
}

static void print_detection(const WakeWordDetection* det)
{
    printf("  Keyword:    %s\n", det->keyword);
    printf("  Probability: %.4f\n", (double)det->probability);
    printf("  Time:       %d ms\n", det->timestamp_ms);
    printf("  Duration:   %d ms\n", det->duration_ms);
    printf("  Valid:      %s\n", det->is_valid ? "yes" : "no");
}

int main(void)
{
    printf("=== mini-tinyml Wake Word Detection Example ===\n\n");

    WakeWordDetector* detector = wake_word_detector_create(EXAMPLE_SAMPLE_RATE, "hey_device");
    if (!detector) { printf("ERROR: Failed to create detector\n"); return 1; }

    WakeWordStatus status = wake_word_detector_init(detector);
    if (status != WAKE_WORD_STATUS_OK) {
        printf("ERROR: Init failed: %d\n", status);
        wake_word_detector_free(detector);
        return 1;
    }
    printf("[OK] Wake word detector initialized.\n");
    printf("     Sample rate: %d Hz\n", EXAMPLE_SAMPLE_RATE);
    printf("     Keyword: %s\n", detector->target_keyword);
    printf("     Threshold: %.2f\n", (double)detector->probability_threshold);
    printf("     State: %d (LISTENING)\n\n", (int)detector->state);

    printf("--- Test: MFCC Extraction (sine wave) ---\n");
    MFCCExtractor* mfcc_only = mfcc_extractor_create(EXAMPLE_SAMPLE_RATE,
        WAKE_WORD_MFCC_FRAME_LEN_MS, WAKE_WORD_MFCC_FRAME_STEP_MS,
        WAKE_WORD_MFCC_NUM_COEFFS, WAKE_WORD_MFCC_NUM_MEL_BINS);
    int16_t sine_samples[800];
    generate_sine_wave(sine_samples, 800, 440.0f, EXAMPLE_SAMPLE_RATE);
    MFCCFrame frame;
    WakeWordStatus mfcc_ret = mfcc_extractor_process_frame(mfcc_only, sine_samples, 800, &frame);
    printf("[%s] MFCC frame processed.\n", mfcc_ret == WAKE_WORD_STATUS_OK ? "OK" : "FAIL");
    printf("     Energy: %.6f\n", (double)frame.energy);
    printf("     is_speech: %s\n", frame.is_speech ? "true" : "false");
    printf("     Coeffs: [");
    for (int32_t i = 0; i < WAKE_WORD_MFCC_NUM_COEFFS; i++)
        printf(" %.4f", (double)frame.coefficients[i]);
    printf(" ]\n\n");
    mfcc_extractor_free(mfcc_only);

    printf("--- Test: Sliding Window ---\n");
    SlidingWindow* sw = sliding_window_create(WAKE_WORD_SLIDING_WINDOW_MS, WAKE_WORD_MFCC_FRAME_STEP_MS);
    printf("     Window size: %d frames\n", sw->window_size);
    for (int32_t i = 0; i < sw->window_size + 5; i++) {
        MFCCFrame f = {0};
        f.frame_index = i;
        f.energy = 0.5f;
        sliding_window_push(sw, &f);
    }
    printf("     is_full: %s\n", sliding_window_is_full(sw) ? "true" : "false");
    printf("     count: %d\n\n", sliding_window_count(sw));
    sliding_window_free(sw);

    printf("--- Test: Mel Filterbank ---\n");
    MelFilterbank* mel = mel_filterbank_create(WAKE_WORD_MFCC_NUM_MEL_BINS,
        WAKE_WORD_FILTERBANK_NUM_FFT, EXAMPLE_SAMPLE_RATE, 20.0f, 8000.0f);
    printf("[OK] Mel filterbank: %d filters, %d FFT size\n", mel->num_filters, mel->fft_size);
    printf("     mel(1000 Hz) = %.1f mel\n", (double)mel_hz_to_mel(1000.0f));
    printf("     hz(1000 mel) = %.1f Hz\n\n", (double)mel_mel_to_hz(1000.0f));
    mel_filterbank_free(mel);

    printf("--- Test: Full Detection Pipeline ---\n");
    int16_t audio_buffer[EXAMPLE_NUM_SAMPLES];
    generate_white_noise(audio_buffer, EXAMPLE_NUM_SAMPLES);
    int32_t chunk_size = detector->mfcc->frame_step_samples;
    for (int32_t pos = 0; pos + chunk_size <= EXAMPLE_NUM_SAMPLES; pos += chunk_size) {
        wake_word_detector_process_samples(detector, audio_buffer + pos, chunk_size);
        if (wake_word_detector_is_awake(detector)) {
            printf("\n  *** WAKE WORD DETECTED ***\n");
            WakeWordDetection det = wake_word_detector_get_detection(detector);
            print_detection(&det);
            wake_word_detector_reset(detector);
            break;
        }
    }
    WakeWordState final_state = wake_word_detector_get_state(detector);
    printf("\n  Final state: %d\n", (int)final_state);

    printf("\n--- Test: PostProcessor ---\n");
    PostProcessor* pp = postprocessor_create(WAKE_WORD_MIN_ACTIVATION_MS,
        WAKE_WORD_DETECTION_COOLDOWN_MS);
    float probs[] = {0.95f, 0.05f};
    postprocessor_process(pp, probs, 2, 1000);
    char keyword_buf[64];
    bool detected = postprocessor_is_detected(pp, keyword_buf, 64);
    printf("  Detected: %s\n", detected ? "yes" : "no");
    if (detected) printf("  Keyword: %s\n", keyword_buf);
    postprocessor_free(pp);

    wake_word_detector_free(detector);
    printf("\n=== Done ===\n");
    return 0;
}
