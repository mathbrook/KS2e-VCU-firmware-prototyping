#ifndef PEDAL_READINGS_H
#define PEDAL_READINGS_H

#define __KS2e_H__
#include <Arduino.h>

class MCU_pedal_readings {
public:
    MCU_pedal_readings() = default;

    MCU_pedal_readings(const uint8_t buf[8]) { load(buf); }

    inline void load(const uint8_t buf[8]) { memcpy(this, buf, sizeof(*this)); }
    inline void write(uint8_t buf[8]) const { memcpy(buf, this, sizeof(*this)); }

    // Getters
    inline uint16_t get_accelerator_pedal_1() const { return accelerator_pedal_1; }
    inline uint16_t get_accelerator_pedal_2() const { return accelerator_pedal_2; }
    inline uint16_t get_brake_transducer_1()  const { return brake_transducer_1; }
    inline uint16_t get_brake_transducer_2()  const { return brake_transducer_2; }

    // Setters
    inline void set_accelerator_pedal_1(uint16_t reading) { accelerator_pedal_1 = reading; }
    inline void set_accelerator_pedal_2(uint16_t reading) { accelerator_pedal_2 = reading; }
    inline void set_brake_transducer_1(uint16_t reading)  { brake_transducer_1  = reading; }
    inline void set_brake_transducer_2(uint16_t reading)  { brake_transducer_2  = reading; }

private:
    // @Parse
    uint16_t accelerator_pedal_1;
    // @Parse
    uint16_t accelerator_pedal_2;
    // @Parse
    uint16_t brake_transducer_1;
    // @Parse
    uint16_t brake_transducer_2;
};

#endif