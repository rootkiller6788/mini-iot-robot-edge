#ifndef WAKE_WORD_H
#define WAKE_WORD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WAKE_WORD_MAX_KEYWORDS          8
#define WAKE_WORD_MAX_PHRASE_LEN        64
#define WAKE_WORD_MFCC_NUM_COEFFS       13
#define WAKE_WORD_MFCC_NUM_MEL_BINS     26
#define WAKE_WORD_MFCC_FRAME_LEN_MS     25
#define WAKE_WORD_MFCC_FRAME_STEP_MS    10
#define WAKE_WORD_DEFAULT_SAMPLE_RATE   16000
#define WAKE_WORD_SLIDING_WINDOW_MS     1000
#define WAKE_WORD_PROB_THRESHOLD        0.85f
#define WAKE_WORD_MIN_ACTIVATION_MS     200
#define WAKE_WORD_DETECTION_COOLDOWN_MS 500
#define WAKE_WORD_MAX_HISTORY_FRAMES    256
#define WAKE_WORD_FILTERBANK_NUM_FFT    512
#define WAKE_WORD_MAX_FRAME_SAMPLES     800

typedef enum {
    WAKE_WORD_STATUS_OK = 0,
    WAKE_WORD_STATUS_ERROR,
    WAKE_WORD_STATUS_INVALID_PARAM,
    WAKE_WORD_STATUS_NOT_INITIALIZED,
    WAKE_WORD_STATUS_MODEL_ERROR,
    WAKE_WORD_STATUS_MEMORY
} WakeWordStatus;

typedef enum {
    WAKE_WORD_STATE_IDLE = 0,
    WAKE_WORD_STATE_LISTENING,
    WAKE_WORD_STATE_DETECTED,
    WAKE_WORD_STATE_ACTIVATED,
    WAKE_WORD_STATE_POST_PROCESSING
} WakeWordState;

typedef enum {
    WAKE_WORD_MODEL_DNN = 0,
    WAKE_WORD_MODEL_CNN,
    WAKE_WORD_MODEL_DS_CNN,
    WAKE_WORD_MODEL_TC_RESNET
} WakeWordModelType;

typedef enum {
    WAKE_WORD_WINDOW_RECTANGULAR = 0,
    WAKE_WORD_WINDOW_HANNING,
    WAKE_WORD_WINDOW_HAMMING
} WakeWordWindowType;

typedef struct {
    float        coefficients[WAKE_WORD_MFCC_NUM_COEFFS];
    float        energy;
    int32_t      frame_index;
    bool         is_speech;
} MFCCFrame;

typedef struct {
    float    filterbank_coeffs[WAKE_WORD_MFCC_NUM_MEL_BINS][WAKE_WORD_FILTERBANK_NUM_FFT];
    int32_t  num_filters;
    int32_t  fft_size;
    int32_t  sample_rate;
    float    low_freq;
    float    high_freq;
    bool     is_initialized;
} MelFilterbank;

typedef struct {
    MFCCFrame          history[WAKE_WORD_MAX_HISTORY_FRAMES];
    int32_t            history_count;
    MelFilterbank*     filterbank;
    int32_t            frame_length_samples;
    int32_t            frame_step_samples;
    int32_t            num_coefficients;
    int32_t            num_mel_bins;
    float              pre_emphasis_coeff;
    WakeWordWindowType window_type;
    float              window_coeffs[WAKE_WORD_MAX_FRAME_SAMPLES];
    bool               is_initialized;
} MFCCExtractor;

typedef struct {
    void*            model_data;
    size_t           model_size;
    WakeWordModelType model_type;
    float            input_scale;
    int32_t          input_zero_point;
    int32_t          input_size;
    int32_t          num_classes;
    char             keyword_labels[WAKE_WORD_MAX_KEYWORDS][32];
    int32_t          num_keywords;
    float            probability_threshold;
    float            detection_smoothing;
    bool             is_loaded;
} WakeWordModel;

typedef struct {
    MFCCFrame frames[WAKE_WORD_MAX_HISTORY_FRAMES];
    int32_t   window_size;
    int32_t   current_index;
    float     probabilities[WAKE_WORD_MAX_KEYWORDS];
    int32_t   max_probability_index;
    float     max_probability_value;
} SlidingWindow;

typedef struct {
    char     keyword[WAKE_WORD_MAX_PHRASE_LEN];
    float    probability;
    int32_t  timestamp_ms;
    int32_t  duration_ms;
    bool     is_valid;
} WakeWordDetection;

typedef struct {
    WakeWordDetection recent_detections[16];
    int32_t           detection_count;
    int32_t           min_activation_ms;
    int32_t           activation_count;
    int32_t           deactivation_count;
    int32_t           last_activation_time;
    int32_t           cooldown_ms;
} PostProcessor;

typedef struct {
    MFCCExtractor*   mfcc;
    WakeWordModel*   model;
    SlidingWindow*   window;
    PostProcessor*   postproc;
    WakeWordState    state;
    int32_t          sample_rate;
    int32_t          current_time_ms;
    float            probability_threshold;
    char             target_keyword[WAKE_WORD_MAX_PHRASE_LEN];
    bool             is_initialized;
    void*            tensor_arena;
    size_t           tensor_arena_size;
} WakeWordDetector;

MFCCExtractor*        mfcc_extractor_create(int32_t sample_rate, int32_t frame_length_ms, int32_t frame_step_ms, int32_t num_coeffs, int32_t num_mel_bins);
void                  mfcc_extractor_free(MFCCExtractor* extractor);
WakeWordStatus        mfcc_extractor_process_frame(MFCCExtractor* extractor, const int16_t* samples, int32_t num_samples, MFCCFrame* out_frame);
void                  mfcc_extractor_reset(MFCCExtractor* extractor);

MelFilterbank*        mel_filterbank_create(int32_t num_filters, int32_t fft_size, int32_t sample_rate, float low_freq, float high_freq);
void                  mel_filterbank_free(MelFilterbank* filterbank);
void                  mel_filterbank_apply(const MelFilterbank* filterbank, const float* power_spectrum, float* mel_energies);
float                 mel_hz_to_mel(float hz);
float                 mel_mel_to_hz(float mel);

WakeWordModel*        wake_word_model_create(void);
void                  wake_word_model_free(WakeWordModel* model);
WakeWordStatus        wake_word_model_load(WakeWordModel* model, const uint8_t* model_bytes, size_t model_bytes_len, WakeWordModelType type);
WakeWordStatus        wake_word_model_predict(const WakeWordModel* model, const float* features, int32_t feature_count, float* probabilities, int32_t num_classes);
WakeWordStatus        wake_word_model_set_threshold(WakeWordModel* model, float threshold);

SlidingWindow*        sliding_window_create(int32_t window_size_ms, int32_t frame_step_ms);
void                  sliding_window_free(SlidingWindow* window);
WakeWordStatus        sliding_window_push(SlidingWindow* window, const MFCCFrame* frame);
WakeWordStatus        sliding_window_get_features(const SlidingWindow* window, float* features, int32_t max_features);
int32_t               sliding_window_count(const SlidingWindow* window);
bool                  sliding_window_is_full(const SlidingWindow* window);
void                  sliding_window_reset(SlidingWindow* window);

PostProcessor*        postprocessor_create(int32_t min_activation_ms, int32_t cooldown_ms);
void                  postprocessor_free(PostProcessor* postproc);
WakeWordStatus        postprocessor_process(PostProcessor* postproc, const float* probabilities, int32_t num_keywords, int32_t timestamp_ms);
bool                  postprocessor_is_detected(const PostProcessor* postproc, char* out_keyword, int32_t max_len);
bool                  postprocessor_ignore_short_activation(PostProcessor* postproc, int32_t current_time_ms);
void                  postprocessor_reset(PostProcessor* postproc);

WakeWordDetector*     wake_word_detector_create(int32_t sample_rate, const char* keyword);
void                  wake_word_detector_free(WakeWordDetector* detector);
WakeWordStatus        wake_word_detector_init(WakeWordDetector* detector);
WakeWordStatus        wake_word_detector_load_model(WakeWordDetector* detector, const uint8_t* model_data, size_t model_size, WakeWordModelType type);
WakeWordStatus        wake_word_detector_process_samples(WakeWordDetector* detector, const int16_t* samples, int32_t num_samples);
bool                  wake_word_detector_is_awake(const WakeWordDetector* detector);
WakeWordDetection     wake_word_detector_get_detection(const WakeWordDetector* detector);
WakeWordState         wake_word_detector_get_state(const WakeWordDetector* detector);
void                  wake_word_detector_set_threshold(WakeWordDetector* detector, float threshold);
void                  wake_word_detector_reset(WakeWordDetector* detector);

#ifdef __cplusplus
}
#endif

#endif /* WAKE_WORD_H */
