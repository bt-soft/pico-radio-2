#include "CwDecoder.h"

// Statikus tagváltozók definíciója
// Goertzel konstansok (az eredeti kódból)
const float CwDecoder::TARGET_FREQ_HZ = 930.0f;
const float CwDecoder::SAMPLING_FREQ_HZ = 8912.0f;  // Az eredeti kód kommentjei alapján
// const short CwDecoder::GOERTZEL_SAMPLES_N = 48; // Már a .h-ban define-ként
short CwDecoder::GOERTZEL_K_CONST;  // Ezt az init()-ben számoljuk
float CwDecoder::GOERTZEL_OMEGA;    // Ezt az init()-ben számoljuk
float CwDecoder::GOERTZEL_COEFF;    // Ezt az init()-ben számoljuk

// Bináris morze fa (az eredeti kódból)
const char CwDecoder::MORSE_CHAR_SYMBOL_TABLE[] = {
    ' ', '5', ' ', 'H', ' ',  '4', ' ', 'S',  // 0
    ' ', ' ', ' ', 'V', ' ',  '3', ' ', 'I',  // 8
    ' ', ' ', ' ', 'F', ' ',  ' ', ' ', 'U',  // 16
    '?', ' ', '_', ' ', ' ',  '2', ' ', 'E',  // 24
    ' ', '&', ' ', 'L', '"',  ' ', ' ', 'R',  // 32
    ' ', '+', '.', ' ', ' ',  ' ', ' ', 'A',  // 40
    ' ', ' ', ' ', 'P', '@',  ' ', ' ', 'W',  // 48
    ' ', ' ', ' ', 'J', '\'', '1', ' ', ' ',  // 56
    ' ', '6', '-', 'B', ' ',  '=', ' ', 'D',  // 64
    ' ', '/', ' ', 'X', ' ',  ' ', ' ', 'N',  // 72
    ' ', ' ', ' ', 'C', ';',  ' ', '!', 'K',  // 80
    ' ', '(', ')', 'Y', ' ',  ' ', ' ', 'T',  // 88
    ' ', '7', ' ', 'Z', ' ',  ' ', ',', 'G',  // 96
    ' ', ' ', ' ', 'Q', ' ',  ' ', ' ', 'M',  // 104
    ':', '8', ' ', ' ', ' ',  ' ', ' ', 'O',  // 112
    ' ', '9', ' ', ' ', ' ',  '0', ' ', ' '   // 120
};

CwDecoder::CwDecoder(uint8_t adc_pin_number)
    : adc_pin_(adc_pin_number),
      current_signal_magnitude_(0.0f),
      signal_detection_threshold_(100.0f),  // Alapértelmezett küszöbérték
      noise_blanker_trigger_level_(1),      // Alapértelmezett zajcsökkentő szint
      no_tone_integrator_counter_(0),
      tone_integrator_counter_(0),
      initial_adaptive_reference_time_ms_(200),  // Alapértelmezett referenciaidő (12 WPM környékén)
      current_adaptive_reference_time_ms_(initial_adaptive_reference_time_ms_),
      current_tone_leading_edge_time_ms_(0),
      current_tone_trailing_edge_time_ms_(0),
      current_detected_tone_element_index_(0),
      current_char_max_tone_duration_ms_(0UL),
      current_char_min_tone_duration_ms_(9999UL),
      morse_decoding_started_flag_(false),
      currently_measuring_tone_duration_flag_(false),
      current_letter_start_time_ms_(0),
      wpm_symbol_counter_(0),
      // calculated_wpm_(0),
      morse_tree_current_nav_index_(63),  // Fa közepe
      morse_tree_nav_offset_val_(32),
      morse_tree_nav_level_count_(6),
      enable_serial_debug_output_(false),
      circular_buffer_write_index_(0),
      circular_buffer_read_index_(0),
      text_buffer_access_lock_instance_(nullptr) {
    // Pico SDK spinlock inicializálása
    uint lock_num = spin_lock_claim_unused(true);
    text_buffer_access_lock_instance_ = spin_lock_init(lock_num);
    // Konstruktor törzse egyébként üres maradhat, az inicializálás az init()-ben történik
}

void CwDecoder::init() {

    // Goertzel konstansok kiszámítása
    GOERTZEL_K_CONST = static_cast<short>(round(static_cast<float>(CW_GOERTZEL_SAMPLES_N) * TARGET_FREQ_HZ / SAMPLING_FREQ_HZ));
    GOERTZEL_OMEGA = (2.0f * PI * static_cast<float>(GOERTZEL_K_CONST)) / static_cast<float>(CW_GOERTZEL_SAMPLES_N);
    GOERTZEL_COEFF = 2.0f * cos(GOERTZEL_OMEGA);

    // Blackman ablak inicializálása (ha használnánk)
    // for (short i = 0; i < CW_GOERTZEL_SAMPLES_N; i++) {
    //   blackman_window_coeffs_[i] = (0.426591f - 0.496561f * cos((2.0f * PI * i) / CW_GOERTZEL_SAMPLES_N) + 0.076848f * cos((4.0f * PI * i) / CW_GOERTZEL_SAMPLES_N));
    // }

    // Kezdeti állapotok
    morse_decoding_started_flag_ = false;
    currently_measuring_tone_duration_flag_ = false;
    current_tone_trailing_edge_time_ms_ = 0;  // Fontos a szóköz detektáláshoz az elején
    reset_morse_tree_navigation_pointers();
    if (enable_serial_debug_output_) {
        Serial.println("CwDecoder initialized.");
        Serial.print("Goertzel K: ");
        Serial.println(GOERTZEL_K_CONST);
        Serial.print("Goertzel Omega: ");
        Serial.println(GOERTZEL_OMEGA);
        Serial.print("Goertzel Coeff: ");
        Serial.println(GOERTZEL_COEFF);
    }
}

void CwDecoder::set_debug_output(bool debug_on) { enable_serial_debug_output_ = debug_on; }

void CwDecoder::set_signal_threshold(float threshold_val) { signal_detection_threshold_ = threshold_val; }

void CwDecoder::set_noise_blanker_level(short level) {
    noise_blanker_trigger_level_ = max((short)0, level);  // Ne legyen negatív
}

void CwDecoder::set_initial_reference_time(unsigned long ref_time_ms) {
    initial_adaptive_reference_time_ms_ = ref_time_ms;
    current_adaptive_reference_time_ms_ = initial_adaptive_reference_time_ms_;
}

void CwDecoder::process_decode_step() {
    bool tone_detected = sample_tone_presence_with_noise_blanker();
    update_decoder_fsm_state(tone_detected);
}

bool CwDecoder::goertzel_filter_detect_tone() {
    for (short i = 0; i < CW_GOERTZEL_SAMPLES_N; i++) {
        goertzel_sample_data_[i] = analogRead(adc_pin_);
        // Nincs explicit késleltetés, az analogRead sebessége és a ciklus adja a mintavételezési frekvenciát.
        // Az eredeti kód kommentjei szerint ez kb. 8912 Hz-et eredményezett az AVR-en.
        // Pico-n ez gyorsabb lehet, finomhangolást igényelhet a SAMPLING_FREQ_HZ és a konstansok.
        // Vagy be kell iktatni egy `delayMicroseconds`t a ciklusba a kívánt SAMPLING_FREQ_HZ eléréséhez.
        // Pl. delayMicroseconds( (1000000 / SAMPLING_FREQ_HZ) - analogRead_execution_time );
        // Ez bonyolult, egyszerűbb lehet a SAMPLING_FREQ_HZ-t a mért értékhez igazítani.
    }

    // Blackman ablak alkalmazása (az eredeti kódban ki van kommentelve)
    // for (short i = 0; i < CW_GOERTZEL_SAMPLES_N; i++) {
    //   goertzel_sample_data_[i] = static_cast<short>(static_cast<float>(goertzel_sample_data_[i]) * blackman_window_coeffs_[i]);
    // }

    goertzel_q1_ = 0;
    goertzel_q2_ = 0;
    for (short i = 0; i < CW_GOERTZEL_SAMPLES_N; i++) {
        goertzel_q0_ = GOERTZEL_COEFF * goertzel_q1_ - goertzel_q2_ + static_cast<float>(goertzel_sample_data_[i]);
        goertzel_q2_ = goertzel_q1_;
        goertzel_q1_ = goertzel_q0_;
    }
    float magnitude_squared = (goertzel_q1_ * goertzel_q1_) + (goertzel_q2_ * goertzel_q2_) - goertzel_q1_ * goertzel_q2_ * GOERTZEL_COEFF;
    current_signal_magnitude_ = sqrt(magnitude_squared);

    if (enable_serial_debug_output_ && (millis() % 1000 < 10)) {  // Csak másodpercenként egyszer írjunk ki
        Serial.print("Mag: "); Serial.println(current_signal_magnitude_);
    }

    return (current_signal_magnitude_ > signal_detection_threshold_);
}

bool CwDecoder::sample_tone_presence_with_noise_blanker() {
    // Az eredeti sample() függvény logikája a noise blankerrel
    // Ez egy blokkoló ciklus, amíg a noise blanker nem dönt.
    // A process_decode_step hívási gyakoriságát ez határozza meg.
    while (true) {  // Ez a while(1) nem ideális, ha a core1-nek más dolga is lenne.
                    // De a morze dekódolás természetéből adódóan a mintavételezésnek folyamatosnak kell lennie.
        if (goertzel_filter_detect_tone()) {
            no_tone_integrator_counter_ = 0;
            tone_integrator_counter_++;
            if (tone_integrator_counter_ > noise_blanker_trigger_level_) return true;
        } else {
            tone_integrator_counter_ = 0;
            no_tone_integrator_counter_++;
            if (no_tone_integrator_counter_ > noise_blanker_trigger_level_) return false;
        }
    }
}

void CwDecoder::update_decoder_fsm_state(bool current_tone_is_present) {
    unsigned long now_ms = millis();

    // 1. Kezdeti hangél várása
    if (!morse_decoding_started_flag_ && !currently_measuring_tone_duration_flag_ && current_tone_is_present) {
        current_tone_leading_edge_time_ms_ = now_ms;
        current_letter_start_time_ms_ = current_tone_leading_edge_time_ms_;

        if (current_tone_trailing_edge_time_ms_ != 0 && (now_ms - current_tone_trailing_edge_time_ms_) > current_adaptive_reference_time_ms_ * 3) {
            add_char_to_output_circular_buffer(' ');
        }
        morse_decoding_started_flag_ = true;
        currently_measuring_tone_duration_flag_ = true;
    }
    // 2. Hang végének várása (hang időtartamának mérése)
    else if (morse_decoding_started_flag_ && currently_measuring_tone_duration_flag_ && !current_tone_is_present) {
        current_tone_trailing_edge_time_ms_ = now_ms;
        unsigned long duration = current_tone_trailing_edge_time_ms_ - current_tone_leading_edge_time_ms_;

        if (current_detected_tone_element_index_ < CW_MAX_TONE_ELEMENTS) {
            if ((current_char_min_tone_duration_ms_ < 9999UL) && (current_char_max_tone_duration_ms_ > 0UL) &&
                (current_char_min_tone_duration_ms_ != current_char_max_tone_duration_ms_)) {
                if (duration < current_adaptive_reference_time_ms_) {  // Pont
                    current_char_min_tone_duration_ms_ = (current_char_min_tone_duration_ms_ + duration) / 2;
                    detected_tone_element_durations_ms_[current_detected_tone_element_index_] = current_char_min_tone_duration_ms_;
                } else {  // Vonal
                    current_char_max_tone_duration_ms_ = (current_char_max_tone_duration_ms_ + duration) / 2;
                    detected_tone_element_durations_ms_[current_detected_tone_element_index_] = current_char_max_tone_duration_ms_;
                }
                current_adaptive_reference_time_ms_ = (current_char_min_tone_duration_ms_ + current_char_max_tone_duration_ms_) / 2;
                if (current_adaptive_reference_time_ms_ == 0) current_adaptive_reference_time_ms_ = initial_adaptive_reference_time_ms_;
            } else {
                detected_tone_element_durations_ms_[current_detected_tone_element_index_] = duration;
                current_char_min_tone_duration_ms_ = min(current_char_min_tone_duration_ms_, detected_tone_element_durations_ms_[current_detected_tone_element_index_]);
                current_char_max_tone_duration_ms_ = max(current_char_max_tone_duration_ms_, detected_tone_element_durations_ms_[current_detected_tone_element_index_]);
            }
            current_detected_tone_element_index_++;
        }
        currently_measuring_tone_duration_flag_ = false;
    }
    // 3. Második és további hangélek várása (karakteren belüli új elem)
    else if (morse_decoding_started_flag_ && !currently_measuring_tone_duration_flag_ && current_tone_is_present) {
        if ((now_ms - current_tone_trailing_edge_time_ms_) < current_adaptive_reference_time_ms_) {
            current_tone_leading_edge_time_ms_ = now_ms;
            currently_measuring_tone_duration_flag_ = true;
        }
    }
    // 4. Időtúllépés (karakter vége)
    else if (morse_decoding_started_flag_ && !currently_measuring_tone_duration_flag_ && !current_tone_is_present) {
        if ((now_ms - current_tone_trailing_edge_time_ms_) > current_adaptive_reference_time_ms_) {
            if (current_detected_tone_element_index_ > 0) {
                if (current_char_max_tone_duration_ms_ != current_char_min_tone_duration_ms_) {
                    current_adaptive_reference_time_ms_ = (current_char_max_tone_duration_ms_ + current_char_min_tone_duration_ms_) / 2;
                    if (current_adaptive_reference_time_ms_ == 0) current_adaptive_reference_time_ms_ = initial_adaptive_reference_time_ms_;
                }

                reset_morse_tree_navigation_pointers();
                wpm_symbol_counter_ = 0;  // WPM-hez

                for (short i = 0; i < current_detected_tone_element_index_; i++) {
                    if (morse_tree_nav_level_count_ < 0) break;
                    if (detected_tone_element_durations_ms_[i] < current_adaptive_reference_time_ms_) {
                        process_dot_signal_input();
                    } else {
                        process_dash_signal_input();
                    }
                }

                if (morse_tree_nav_level_count_ >= 0 && morse_tree_current_nav_index_ >= 0 &&
                    morse_tree_current_nav_index_ < (sizeof(MORSE_CHAR_SYMBOL_TABLE) / sizeof(MORSE_CHAR_SYMBOL_TABLE[0]))) {
                    char decoded_char = MORSE_CHAR_SYMBOL_TABLE[morse_tree_current_nav_index_];
                    if (decoded_char != ' ') {  // Ne írjunk ki üres helykitöltő karaktereket a fából
                        add_char_to_output_circular_buffer(decoded_char);
                    }
                }
                // WPM számítás (az eredeti kódból, de itt nincs aktívan használva)
                // if (current_tone_trailing_edge_time_ms_ > current_letter_start_time_ms_ && wpm_symbol_counter_ > 1) {
                //    calculated_wpm_ = ((wpm_symbol_counter_ - 1) * 1200) / (current_tone_trailing_edge_time_ms_ - current_letter_start_time_ms_);
                // }
            }
            morse_decoding_started_flag_ = false;
            currently_measuring_tone_duration_flag_ = false;
            current_detected_tone_element_index_ = 0;
            current_char_min_tone_duration_ms_ = 9999UL;
            current_char_max_tone_duration_ms_ = 0UL;
        }
    }
}

void CwDecoder::reset_morse_tree_navigation_pointers() {
    morse_tree_current_nav_index_ = 63;
    morse_tree_nav_offset_val_ = 32;
    morse_tree_nav_level_count_ = 6;
}

void CwDecoder::handle_morse_decoding_error() {
    if (enable_serial_debug_output_) Serial.println("CW Decoder: Error in dot/dash processing!");
    reset_morse_tree_navigation_pointers();
    current_adaptive_reference_time_ms_ = initial_adaptive_reference_time_ms_;
    current_char_min_tone_duration_ms_ = 9999UL;
    current_char_max_tone_duration_ms_ = 0L;
    current_detected_tone_element_index_ = 0;
    wpm_symbol_counter_ = 0;

    morse_decoding_started_flag_ = false;
    currently_measuring_tone_duration_flag_ = false;
}

void CwDecoder::process_dot_signal_input() {
    if (morse_tree_nav_level_count_ < 0) return;

    morse_tree_current_nav_index_ -= morse_tree_nav_offset_val_;
    morse_tree_nav_offset_val_ /= 2;
    morse_tree_nav_level_count_--;
    wpm_symbol_counter_ += 2;  // 1*dot + 1*space

    if (morse_tree_nav_level_count_ < 0) {
        handle_morse_decoding_error();
    }
}

void CwDecoder::process_dash_signal_input() {
    if (morse_tree_nav_level_count_ < 0) return;

    morse_tree_current_nav_index_ += morse_tree_nav_offset_val_;
    morse_tree_nav_offset_val_ /= 2;
    morse_tree_nav_level_count_--;
    wpm_symbol_counter_ += 4;  // 3*dots + 1*space

    if (morse_tree_nav_level_count_ < 0) {
        handle_morse_decoding_error();
    }
}

void CwDecoder::lock_text_buffer_access() {
    // Pico SDK spinlock használata - ez letiltja az interruptokat a jelenlegi magon
    spin_lock_unsafe_blocking(text_buffer_access_lock_instance_);
}

void CwDecoder::unlock_text_buffer_access() { spin_unlock_unsafe(text_buffer_access_lock_instance_); }

void CwDecoder::add_char_to_output_circular_buffer(char c) {
    lock_text_buffer_access();

    decoded_char_circular_buffer_[circular_buffer_write_index_] = c;
    circular_buffer_write_index_ = (circular_buffer_write_index_ + 1) % CW_DECODER_BUFFER_SIZE;

    // Ha a buffer tele lett és felülírtunk egy még ki nem olvasott karaktert ("kicsorgás")
    if (circular_buffer_write_index_ == circular_buffer_read_index_) {
        circular_buffer_read_index_ = (circular_buffer_read_index_ + 1) % CW_DECODER_BUFFER_SIZE;  // A legrégebbi elvész
    }

    unlock_text_buffer_access();
    if (enable_serial_debug_output_) {
        // Ezt a kiírást a core0-nak kellene végeznie, hogy ne zavarja a core1 időzítését.
        // De debug célból itt hagyhatjuk, ha a Serial objektum thread-safe (Arduino Pico-n általában az).
        // Vagy egy külön debug puffert használhatnánk.
        // Most egyszerűen kiírjuk, feltételezve, hogy a Serial kezelése nem okoz nagy problémát.
        //Serial.print(c);  // Ezt inkább a get_decoded_text után tegyük a fő ciklusba
    }
}

size_t CwDecoder::get_decoded_text(char* buffer, size_t max_len) {
    if (max_len == 0) return 0;

    lock_text_buffer_access();

    size_t chars_read = 0;
    while (circular_buffer_read_index_ != circular_buffer_write_index_ && chars_read < (max_len - 1)) {
        buffer[chars_read++] = decoded_char_circular_buffer_[circular_buffer_read_index_];
        circular_buffer_read_index_ = (circular_buffer_read_index_ + 1) % CW_DECODER_BUFFER_SIZE;
    }
    buffer[chars_read] = '\0';  // Null terminátor

    unlock_text_buffer_access();
    return chars_read;
}
