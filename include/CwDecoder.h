#ifndef CW_DECODER_H
#define CW_DECODER_H

#include <Arduino.h>    // Szükséges az Arduino specifikus függvényekhez (millis, analogRead, stb.)
#include <pico/sync.h>  // Pico SDK spinlock-hoz

#include <cmath>  // round, sin, cos, sqrt függvényekhez

#define CW_DECODER_BUFFER_SIZE 256  // A dekódolt szöveg körkörös pufferének mérete
#define CW_MAX_TONE_ELEMENTS 6      // Maximális pont/vonal szám egy karakterben (az eredeti kód alapján)
#define CW_GOERTZEL_SAMPLES_N 48    // Goertzel minták száma

class CwDecoder {
   public:
    CwDecoder(uint8_t adc_pin_number);  // Konstruktor, megkapja az ADC bemeneti pin számát (pl. A0, A1)

    void init();  // Fő inicializálás, a core0 setup() függvényéből hívandó

    // Ezt a metódust hívja a core1 loop1() függvénye folyamatosan
    void process_decode_step();

    // A fő mag (core0) hívja ezt a dekódolt szöveg lekéréséhez
    // Visszaadja a kiolvasott karakterek számát.
    // A kiolvasott karakterek törlődnek a belső pufferből.
    size_t get_decoded_text(char* buffer, size_t max_len);

    void set_debug_output(bool debug_on);
    void set_signal_threshold(float threshold_val);
    void set_noise_blanker_level(short level);
    void set_initial_reference_time(unsigned long ref_time_ms);

   private:
    // Goertzel algoritmus és mintavételezés
    bool sample_tone_presence_with_noise_blanker();  // Az eredeti sample() logika
    bool goertzel_filter_detect_tone();              // Az eredeti goertzel() logika

    // Morze fa bejárása
    void process_dot_signal_input();
    void process_dash_signal_input();
    void reset_morse_tree_navigation_pointers();
    void handle_morse_decoding_error();

    // Állapotgép logika (az eredeti loop() függvényből adaptálva)
    void update_decoder_fsm_state(bool current_tone_is_present);

    // Körkörös puffer kezelése
    void add_char_to_output_circular_buffer(char c);

    // Tagváltozók
    uint8_t adc_pin_;  // Arduino ADC pin (pl. A0, A1)

    // Goertzel paraméterek
    static const float TARGET_FREQ_HZ;
    static const float SAMPLING_FREQ_HZ;
    // static const short GOERTZEL_SAMPLES_N; // Már define-ként fentebb
    static short GOERTZEL_K_CONST;
    static float GOERTZEL_OMEGA;
    // static float GOERTZEL_SINE; // Nincs használva a kódban
    // static float GOERTZEL_COSINE; // Nincs használva a kódban
    static float GOERTZEL_COEFF;

    float goertzel_q0_, goertzel_q1_, goertzel_q2_;
    short goertzel_sample_data_[CW_GOERTZEL_SAMPLES_N];
    // float blackman_window_coeffs_[CW_GOERTZEL_SAMPLES_N]; // Az eredeti kódban a használata ki van kommentelve

    float current_signal_magnitude_;
    float signal_detection_threshold_;

    // Zajcsökkentő (Noise Blanker)
    short noise_blanker_trigger_level_;
    int no_tone_integrator_counter_;
    int tone_integrator_counter_;

    // Morze időzítés és állapot
    unsigned long initial_adaptive_reference_time_ms_;
    unsigned long current_adaptive_reference_time_ms_;
    unsigned long current_tone_leading_edge_time_ms_;
    unsigned long current_tone_trailing_edge_time_ms_;
    unsigned long detected_tone_element_durations_ms_[CW_MAX_TONE_ELEMENTS];
    short current_detected_tone_element_index_;
    unsigned long current_char_max_tone_duration_ms_;
    unsigned long current_char_min_tone_duration_ms_;

    bool morse_decoding_started_flag_;
    bool currently_measuring_tone_duration_flag_;

    // WPM számításhoz (az eredeti kódból, de itt nincs aktívan használva a kimeneten)
    unsigned long current_letter_start_time_ms_;
    short wpm_symbol_counter_;
    // short calculated_wpm_;

    // Bináris morze fa
    static const char MORSE_CHAR_SYMBOL_TABLE[];
    short morse_tree_current_nav_index_;
    short morse_tree_nav_offset_val_;
    short morse_tree_nav_level_count_;

    // Debug
    bool enable_serial_debug_output_;

    // Körkörös puffer a dekódolt karaktereknek és a "spinlock"
    char decoded_char_circular_buffer_[CW_DECODER_BUFFER_SIZE];
    volatile uint32_t circular_buffer_write_index_;
    volatile uint32_t circular_buffer_read_index_;
    spin_lock_t* text_buffer_access_lock_instance_;  // Pico SDK spinlock

    void lock_text_buffer_access();
    void unlock_text_buffer_access();
};

#endif  // CW_DECODER_H
