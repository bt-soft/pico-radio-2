#include "RttyDecoder.h"

#include <cstdio>  // printf-hez (debug)

// Baudot kód táblák (az ITA2 szabvány alapján)
const char RttyDecoder::BAUDOT_LTRS_TABLE[32] = {
    '\0', 'E', '\n', 'A', ' ', 'S', 'I', 'U', '\r', 'D', 'R', 'J',  'N', 'F', 'C', 'K',
    'T',  'Z', 'L',  'W', 'H', 'Y', 'P', 'Q', 'O',  'B', 'G', '\0', 'M', 'X', 'V', '\0'  // FIGS (0x1B), LTRS (0x1F)
};  // A 0. indexű '\0' a "Blank" vagy "Null" karakternek felel meg.

const char RttyDecoder::BAUDOT_FIGS_TABLE[32] = {
    '\0', '3', '\n', '-', ' ', '\'', '8', '7', '\r', '$', '4', '\a', ',', '!', ':', '(',  // Bell (0x0B) -> '\a'
    '5',  '+', ')',  '2', '#', '6',  '0', '1', '9',  '?', '&', '\0', '.', '/', ';', '\0'  // FIGS (0x1B), LTRS (0x1F)
};  // A 0. indexű '\0' a "Blank" vagy "Null" karakternek felel meg.

RttyDecoder::RttyDecoder(uint8_t adc_pin_number)
    : adc_pin_(adc_pin_number),
      prev_analog_value_(0),
      current_rtty_state_(RttyState::IDLE),
      last_bit_sample_time_us_(0),
      bits_received_count_(0),
      current_char_bits_(0),
      current_shift_is_letters_(true),  // Kezdetben Letters shift
      baud_rate_(RTTY_BAUD_RATE),
      // mark_freq_target_(RTTY_MARK_FREQ_TARGET),
      // space_freq_target_(RTTY_SPACE_FREQ_TARGET),
      zc_window_size_(RTTY_ZC_WINDOW_SIZE),
      zc_threshold_(RTTY_ZC_THRESHOLD),
      bit_time_us_(RTTY_BIT_TIME_US),
      half_bit_time_us_(RTTY_HALF_BIT_TIME_US),
      enable_serial_debug_output_(false),
      circular_buffer_write_index_(0),
      circular_buffer_read_index_(0),
      text_buffer_access_lock_instance_(nullptr) {
    // Pico SDK spinlock inicializálása
    uint lock_num = spin_lock_claim_unused(true);
    text_buffer_access_lock_instance_ = spin_lock_init(lock_num);
}

void RttyDecoder::init() {
    pinMode(adc_pin_, INPUT);  // ADC pin beállítása bemenetként
    // Az analogReadResolution() beállítása a fő setup()-ban történik általában.

    // Kezdeti analogRead érték a nullátmenet detektáláshoz
    prev_analog_value_ = analogRead(adc_pin_) - 2048;  // Középpontra igazítva

    // Kezdeti állapotok
    current_rtty_state_ = RttyState::IDLE;
    last_bit_sample_time_us_ = micros();  // Kezdeti idő
    bits_received_count_ = 0;
    current_char_bits_ = 0;
    current_shift_is_letters_ = true;  // RTTY általában Letters shift-tel indul

    if (enable_serial_debug_output_) {
        Serial.println("RttyDecoder initialized.");
        Serial.print("Baud Rate: ");
        Serial.println(baud_rate_);
        Serial.print("ZC Window: ");
        Serial.println(zc_window_size_);
        Serial.print("ZC Threshold: ");
        Serial.println(zc_threshold_);
        Serial.print("Bit Time (us): ");
        Serial.println(bit_time_us_);
    }
}

void RttyDecoder::set_debug_output(bool debug_on) { enable_serial_debug_output_ = debug_on; }

void RttyDecoder::set_baud_rate(float baud) {
    baud_rate_ = baud;
    bit_time_us_ = (unsigned long)(1000000.0f / baud_rate_);
    half_bit_time_us_ = bit_time_us_ / 2;
}

void RttyDecoder::set_zc_window_size(uint16_t size) { zc_window_size_ = size; }

void RttyDecoder::set_zc_threshold(uint16_t threshold) { zc_threshold_ = threshold; }

void RttyDecoder::process_decode_step() {
    uint16_t zc_count = get_zero_crossing_count();
    bool current_input_is_mark = is_mark_state_from_zc(zc_count);  // True ha Mark, False ha Space
    unsigned long now_us = micros();

    // Debug kiírás (ritkítva)
    if (/*enable_serial_debug_output_ &&*/ (now_us % 100000 < 100)) {  // Kb. 10x másodpercenként
        Serial.print("ZC: ");
        Serial.print(zc_count);
        Serial.print(current_input_is_mark ? " MARK " : " SPACE ");
        Serial.print("State: ");
        // Olvashatóbb kiírás:
        switch (current_rtty_state_) {
            case RttyState::IDLE:
                Serial.println("IDLE");
                break;
            case RttyState::START_BIT_CONFIRM:
                Serial.println("START_BIT_CONFIRM");
                break;
            case RttyState::DATA_BITS:
                Serial.println("DATA_BITS");
                break;
            case RttyState::STOP_BITS:
                Serial.println("STOP_BITS");
                break;
            default:
                Serial.println("UNKNOWN_STATE");
                break;  // Nem szabadna ide jutni
        }
    }

    switch (current_rtty_state_) {
        case RttyState::IDLE:
            // Várakozás start bitre (Space állapot)
            if (!current_input_is_mark) {  // Ha Space-t érzékelünk
                current_rtty_state_ = RttyState::START_BIT_CONFIRM;
                last_bit_sample_time_us_ = now_us;  // Potenciális start bit kezdete
                if (enable_serial_debug_output_) Serial.println("RTTY: Potential Start bit (Space).");
            }
            break;

        case RttyState::START_BIT_CONFIRM:
            // Megerősítjük a start bitet: továbbra is Space van-e kb. fél bitidő múlva?
            if (now_us - last_bit_sample_time_us_ >= half_bit_time_us_) {
                if (!current_input_is_mark) {  // Még mindig Space -> érvényes start bit
                    current_rtty_state_ = RttyState::DATA_BITS;
                    // Az első adatbit mintavételezési ideje a start bit végétől (last_bit_sample_time_us_)
                    // számított fél bitidő múlva lesz.
                    // A last_bit_sample_time_us_ most a start bit kezdetét jelöli.
                    // Az első adatbit mintavételezési pontja: start_bit_kezdete + 1.5 * bit_idő
                    last_bit_sample_time_us_ = last_bit_sample_time_us_ + bit_time_us_ + half_bit_time_us_;
                    bits_received_count_ = 0;
                    current_char_bits_ = 0;
                    if (enable_serial_debug_output_) Serial.println("RTTY: Start bit confirmed. Moving to Data bits.");
                } else {  // Visszaváltott Mark-ra -> téves riasztás, vissza IDLE-be
                    current_rtty_state_ = RttyState::IDLE;
                    if (enable_serial_debug_output_) Serial.println("RTTY: False Start bit (was Mark). Back to IDLE.");
                }
            }
            break;

        case RttyState::DATA_BITS:
            // Adatbitek fogadása (5 bit)
            if (now_us >= last_bit_sample_time_us_) {
                // Bit mintavételezési idő
                uint8_t bit_value = current_input_is_mark ? 1 : 0;  // Mark = 1, Space = 0
                current_char_bits_ |= (bit_value << bits_received_count_);
                bits_received_count_++;

                if (enable_serial_debug_output_) {
                    Serial.print("RTTY: Data bit ");
                    Serial.print(bits_received_count_);
                    Serial.print(" val: ");
                    Serial.print(bit_value);
                    Serial.print(" (char_bits: 0x");
                    Serial.print(current_char_bits_, HEX);
                    Serial.println(")");
                }

                if (bits_received_count_ >= 5) {
                    // Mind az 5 adatbit fogadva
                    current_rtty_state_ = RttyState::STOP_BITS;
                    // A stop bit mintavételezési ideje az utolsó adatbit mintavételezési idejétől
                    // számított 1 bitidő múlva lesz.
                    last_bit_sample_time_us_ = last_bit_sample_time_us_ + bit_time_us_;
                    if (enable_serial_debug_output_) {
                        Serial.print("RTTY: All 5 data bits received. Char bits: 0x");
                        Serial.println(current_char_bits_, HEX);
                    }
                } else {
                    // Következő adatbit mintavételezési ideje
                    last_bit_sample_time_us_ = last_bit_sample_time_us_ + bit_time_us_;
                }
            }
            break;

        case RttyState::STOP_BITS:
            // Stop bit(ek) fogadása (Mark állapotnak kell lennie)
            // Az RTTY általában 1.5 vagy 2 stop bitet használ.
            // Itt egyszerűen várunk kb. 1.5 bitidőt az utolsó adatbit mintavételezése után,
            // és ellenőrizzük, hogy Mark állapotban van-e a vonal.
            if (now_us >= last_bit_sample_time_us_) {  // Legalább 1 stop bit eltelt
                if (current_input_is_mark) {           // Stop bit érvényes (Mark)
                    char decoded_char = decode_baudot_char(current_char_bits_);
                    if (decoded_char != '\0') {  // Csak érvényes karaktereket adjunk hozzá
                        add_char_to_output_circular_buffer(decoded_char);
                    }
                    if (enable_serial_debug_output_) {
                        Serial.print("RTTY: Stop bit(s) OK (Mark). Decoded: '");
                        if (decoded_char >= ' ' && decoded_char <= '~')
                            Serial.print(decoded_char);
                        else
                            Serial.print("0x");
                        Serial.print((int)decoded_char, HEX);
                        Serial.println("'");
                    }
                } else {
                    if (enable_serial_debug_output_) Serial.println("RTTY: Framing Error (Stop bit was Space).");
                    // Hiba esetén is visszaállunk IDLE-be, de nem dekódolunk karaktert.
                }
                current_rtty_state_ = RttyState::IDLE;  // Vissza IDLE állapotba
            }
            break;
    }
}

uint16_t RttyDecoder::get_zero_crossing_count() {
    uint16_t zc_count = 0;
    int current_analog_value;
    // Az ADC középpontja kb. 2048 (12 bites ADC esetén).
    // A bemeneti jelnek AC csatoltnak kell lennie, hogy 0V (azaz ADC 2048) körül ingadozzon.
    const int adc_center = 2048;

    for (uint16_t i = 0; i < zc_window_size_; ++i) {
        current_analog_value = analogRead(adc_pin_) - adc_center;  // Középpontra igazítás

        // Nullátmenet detektálás: ha az előző és az aktuális érték előjele eltér
        if ((prev_analog_value_ > 0 && current_analog_value <= 0) || (prev_analog_value_ < 0 && current_analog_value >= 0)) {
            zc_count++;
        }
        prev_analog_value_ = current_analog_value;

        // Nagyon rövid késleltetés a mintavételezési frekvencia stabilizálásához, ha szükséges.
        // A Pico analogRead() gyors, ezért a ciklus sebessége magas lehet.
        // A ZC_WINDOW_SIZE és a ciklus futási ideje határozza meg, mennyi ideig mérünk.
        // Pl. ha 100 minta és minden minta 10us (ADC + ciklus), akkor 1ms-ig mérünk.
        // delayMicroseconds(1); // Finomhangolást igényelhet a tényleges ADC sebesség alapján
    }
    return zc_count;
}

bool RttyDecoder::is_mark_state_from_zc(uint16_t zc_count) {
    // Magasabb frekvencia (Space) több nullátmenetet eredményez egy adott időablakban.
    // Alacsonyabb frekvencia (Mark) kevesebb nullátmenetet eredményez.
    // Ha a nullátmenetek száma a küszöb ALATT van, az Mark. Ha FELETTE, az Space.
    // A küszöböt (zc_threshold_) kalibrálni kell!
    return zc_count < zc_threshold_;  // True, ha Mark (kevesebb ZC)
}

char RttyDecoder::decode_baudot_char(uint8_t five_bits) {
    // A Baudot kód 5 bites, a five_bits változó ezt tartalmazza (0-31).
    // A legkisebb helyiértékű bit (LSB) jön először.
    // A táblázatok az LSB-first sorrendet követik.

    // Shift karakterek kezelése
    if (five_bits == 0x1F) {  // LTRS (Letters Shift)
        current_shift_is_letters_ = true;
        if (enable_serial_debug_output_) Serial.println("RTTY: Shift to Letters.");
        return '\0';                 // Shift karaktert nem írunk ki
    } else if (five_bits == 0x1B) {  // FIGS (Figures Shift)
        current_shift_is_letters_ = false;
        if (enable_serial_debug_output_) Serial.println("RTTY: Shift to Figures.");
        return '\0';  // Shift karaktert nem írunk ki
    }

    // Adat karakter dekódolása
    if (current_shift_is_letters_) {
        return BAUDOT_LTRS_TABLE[five_bits];
    } else {
        return BAUDOT_FIGS_TABLE[five_bits];
    }
}

void RttyDecoder::lock_text_buffer_access() { spin_lock_unsafe_blocking(text_buffer_access_lock_instance_); }

void RttyDecoder::unlock_text_buffer_access() { spin_unlock_unsafe(text_buffer_access_lock_instance_); }

void RttyDecoder::add_char_to_output_circular_buffer(char c) {
    // Speciális, nem nyomtatható karakterek kezelése (opcionális)
    // Pl. CR, LF, BELL
    if (c == '\r') {        /* CR kezelése, ha szükséges, pl. sor elejére ugrás a kijelzőn */
    } else if (c == '\n') { /* LF kezelése, pl. új sor a kijelzőn */
    } else if (c == '\a') { /* BELL kezelése, pl. hangjelzés */
    }
    // A többi karaktert (beleértve a szóközt is) hozzáadjuk a pufferhez.
    // A '\0' (Blank/Null) karaktereket a decode_baudot_char már kiszűrte a shift karakterekkel együtt.

    if (c < ' ' && c != '\n' && c != '\r') {  // Csak nyomtatható karaktereket és CR/LF-et engedünk át
        if (enable_serial_debug_output_) {
            Serial.print("RTTY: Non-printable char 0x");
            Serial.print((int)c, HEX);
            Serial.println(" skipped.");
        }
        return;
    }

    lock_text_buffer_access();

    decoded_char_circular_buffer_[circular_buffer_write_index_] = c;
    circular_buffer_write_index_ = (circular_buffer_write_index_ + 1) % RTTY_DECODER_BUFFER_SIZE;

    if (circular_buffer_write_index_ == circular_buffer_read_index_) {
        circular_buffer_read_index_ = (circular_buffer_read_index_ + 1) % RTTY_DECODER_BUFFER_SIZE;
    }

    unlock_text_buffer_access();
    // A debug kiírást a core0-nak kellene végeznie a get_decoded_text után.
}

size_t RttyDecoder::get_decoded_text(char* buffer, size_t max_len) {
    if (max_len == 0) return 0;

    lock_text_buffer_access();

    size_t chars_read = 0;
    while (circular_buffer_read_index_ != circular_buffer_write_index_ && chars_read < (max_len - 1)) {
        buffer[chars_read++] = decoded_char_circular_buffer_[circular_buffer_read_index_];
        circular_buffer_read_index_ = (circular_buffer_read_index_ + 1) % RTTY_DECODER_BUFFER_SIZE;
    }
    buffer[chars_read] = '\0';  // Null terminátor

    unlock_text_buffer_access();
    return chars_read;
}
