#ifndef RTTY_DECODER_H
#define RTTY_DECODER_H

#include <Arduino.h>    // Szükséges az Arduino specifikus függvényekhez (millis, analogRead, stb.)
#include <pico/sync.h>  // Pico SDK spinlock-hoz

// RTTY specifikus konstansok (finomhangolást igényelhetnek a te bemeneti jelszintedhez és a Pico ADC sebességéhez)
#define RTTY_BAUD_RATE 45.45f
// A Mark/Space frekvenciák és a ZC paraméterek a drajnauth repositoryból származnak,
// de a Pico ADC/timing miatt módosításra és kalibrációra szorulhatnak!
#define RTTY_MARK_FREQ_TARGET 2125.0f   // Cél Mark frekvencia
#define RTTY_SPACE_FREQ_TARGET 2295.0f  // Cél Space frekvencia (170 Hz shift)

// Zero Crossing detektálás paraméterei
#define RTTY_ZC_WINDOW_SIZE 100  // Minták száma a nullátmenet ablakban
#define RTTY_ZC_THRESHOLD 10     // Nullátmenet küszöb (kalibrálandó!) - Ez választja el a Mark-ot a Space-től

#define RTTY_BIT_TIME_US (unsigned long)(1000000.0f / RTTY_BAUD_RATE)  // Egy bit időtartama mikroszekundumban
#define RTTY_HALF_BIT_TIME_US (RTTY_BIT_TIME_US / 2)                   // Fél bit időtartama

#define RTTY_DECODER_BUFFER_SIZE 256  // A dekódolt szöveg körkörös pufferének mérete

class RttyDecoder {
   public:
    RttyDecoder(uint8_t adc_pin_number);  // Konstruktor, megkapja az ADC bemeneti pin számát (pl. A0, A1)

    void init();  // Fő inicializálás, a core0 setup() függvényéből hívandó

    // Ezt a metódust hívja a core1 loop1() függvénye folyamatosan
    void process_decode_step();

    // A fő mag (core0) hívja ezt a dekódolt szöveg lekéréséhez
    // Visszaadja a kiolvasott karakterek számát.
    // A kiolvasott karakterek törlődnek a belső pufferből.
    size_t get_decoded_text(char* buffer, size_t max_len);

    void set_debug_output(bool debug_on);
    // Lehetőségek a paraméterek futásidejű állítására (opcionális)
    void set_baud_rate(float baud);
    // void set_mark_freq(float freq); // A ZC módszer nem használja közvetlenül a frekvenciákat, csak a ZC számot
    // void set_space_freq(float freq);
    void set_zc_window_size(uint16_t size);
    void set_zc_threshold(uint16_t threshold);

   private:
    // Zero Crossing alapú frekvencia detektálás
    uint16_t get_zero_crossing_count();             // Nullátmenetek számolása egy ablakban
    bool is_mark_state_from_zc(uint16_t zc_count);  // Nullátmenet szám alapján Mark/Space eldöntése

    // RTTY dekódolás állapotgép és logika
    enum class RttyState {
        IDLE,               // Várakozás start bitre (Space)
        START_BIT_CONFIRM,  // Start bit megerősítése
        DATA_BITS,          // Adatbitek fogadása (5 bit)
        STOP_BITS           // Stop bit(ek) fogadása (Mark)
    };
    RttyState current_rtty_state_;
    unsigned long last_bit_sample_time_us_;  // Utolsó bit mintavételezés ideje mikroszekundumban
    uint8_t bits_received_count_;            // Fogadott adatbitek száma
    uint8_t current_char_bits_;              // Az 5 fogadott adatbit

    bool current_shift_is_letters_;  // Aktuális shift állapot (true = Letters, false = Figures)

    // Baudot kód dekódolása
    char decode_baudot_char(uint8_t five_bits);
    static const char BAUDOT_LTRS_TABLE[32];  // Baudot Letters tábla
    static const char BAUDOT_FIGS_TABLE[32];  // Baudot Figures tábla

    // Tagváltozók
    uint8_t adc_pin_;        // Arduino ADC pin (pl. A0, A1)
    int prev_analog_value_;  // Előző analogRead érték a nullátmenet detektáláshoz

    // RTTY paraméterek (futásidőben állíthatók)
    float baud_rate_;
    // float mark_freq_target_; // A ZC módszer nem használja közvetlenül
    // float space_freq_target_;
    uint16_t zc_window_size_;
    uint16_t zc_threshold_;
    unsigned long bit_time_us_;
    unsigned long half_bit_time_us_;

    // Debug
    bool enable_serial_debug_output_;

    // Körkörös puffer a dekódolt karaktereknek és a "spinlock"
    char decoded_char_circular_buffer_[RTTY_DECODER_BUFFER_SIZE];
    volatile uint32_t circular_buffer_write_index_;
    volatile uint32_t circular_buffer_read_index_;
    spin_lock_t* text_buffer_access_lock_instance_;  // Pico SDK spinlock

    void lock_text_buffer_access();
    void unlock_text_buffer_access();
    void add_char_to_output_circular_buffer(char c);
};

#endif  // RTTY_DECODER_H
