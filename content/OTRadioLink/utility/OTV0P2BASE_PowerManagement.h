/*
The OpenTRV project licenses this file to you
under the Apache Licence, Version 2.0 (the "Licence");
you may not use this file except in compliance
with the Licence. You may obtain a copy of the Licence at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the Licence is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied. See the Licence for the
specific language governing permissions and limitations
under the Licence.

Author(s) / Copyright (s): Damon Hart-Davis 2013--2016
                           Deniz Erbilgin 2015
*/

/*
  Utilities to assist with minimal power usage on V0p2 boards,
  including interrupts and sleep.
  */

#ifndef OTV0P2BASE_POWERMANAGEMENT_H
#define OTV0P2BASE_POWERMANAGEMENT_H

#include <stdint.h>
#include <util/atomic.h>
#include <Arduino.h>

#include "OTV0P2BASE_BasicPinAssignments.h"
#include "OTV0P2BASE_FastDigitalIO.h"

#include "OTV0P2BASE_Sensor.h"

namespace OTV0P2BASE
{


// Call from setup() for V0p2 board to turn off unused modules, set up timers and interrupts, etc.
// I/O pin setting is not done here.
void powerSetup();

// Selectively turn off all modules that need not run continuously on V0p2 board
// so as to minimise power without (ie over and above) explicitly entering a sleep mode.
// Suitable for start-up and for belt-and-braces use before main sleep on each cycle,
// to ensure that nothing power-hungry is accidentally left on.
// Any module that may need to run all the time should not be turned off here.
// May be called from panic(), so do not be too clever.
// Does NOT attempt to power down the radio, eg in case that needs to be left in RX mode.
// Does NOT attempt to power down the hardware serial/UART.
void minimisePowerWithoutSleep();


// If ADC was disabled, power it up, do Serial.begin(), and return true.
// If already powered up then do nothing other than return false.
// This does not power up the analogue comparator; this needs to be manually enabled if required.
// If this returns true then a matching powerDownADC() may be advisable.
bool powerUpADCIfDisabled();
// Power ADC down.
// Likely shorter inline than just the call/return!
inline void powerDownADC()
  {
  ADCSRA &= ~_BV(ADEN); // Do before power_[adc|all]_disable() to avoid freezing the ADC in an active state!
  PRR |= _BV(PRADC); // Disable the ADC.
  }


// If true, default is to run the SPI bus a bit below maximum (eg for REV2 board).
static const bool DEFAULT_RUN_SPI_SLOW = false;

// TEMPLATED DEFINITIONS OF SPI power up/down.
//
// If SPI was disabled, power it up, enable it as master and with a sensible clock speed, etc, and return true.
// If already powered up then do nothing other than return false.
// If this returns true then a matching powerDownSPI() may be advisable.
// The optional slowSPI flag, if true, attempts to run the bus slow, eg for when long or loaded with LED on SCK.
template <uint8_t SPI_nSS, bool slowSPI>
bool t_powerUpSPIIfDisabled()
    {
    ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
        {
        if(!(PRR & _BV(PRSPI))) { return(false); }

        // Ensure that nSS is HIGH ASAP and thus any slave deselected when powering up SPI.
        fastDigitalWrite(SPI_nSS, HIGH);
        // Ensure that nSS is an output to avoid forcing SPI to slave mode by accident.
        pinMode(SPI_nSS, OUTPUT);

        PRR &= ~_BV(PRSPI); // Enable SPI power.

        // Configure raw SPI to match better how it was used in PICAXE V0.09 code.
        // CPOL = 0, CPHA = 0
        // Enable SPI, set master mode, set speed.
        const uint8_t ENABLE_MASTER = _BV(SPE) | _BV(MSTR);
#if F_CPU <= 2000000 // Needs minimum prescale (x2) with slow (<=2MHz) CPU clock.
        SPCR = ENABLE_MASTER; // 2x clock prescale for <=1MHz SPI clock from <=2MHz CPU clock (500kHz SPI @ 1MHz CPU).
        if(!slowSPI) { SPSR = _BV(SPI2X); } // Slow will give 4x prescale for 250kHz bus at 1MHz CPU.
#elif F_CPU <= 8000000
        SPCR = ENABLE_MASTER; // 4x clock prescale for <=2MHz SPI clock from nominal <=8MHz CPU clock.
        SPSR = 0;
#else // Needs setting for fast (~16MHz) CPU clock.
        SPCR = _BV(SPR0) | ENABLE_MASTER; // 8x clock prescale for ~2MHz SPI clock from nominal ~16MHz CPU clock.
        SPSR = _BV(SPI2X);
#endif
        }
    return(true);
    }
//
// Power down SPI.
template <uint8_t SPI_nSS, uint8_t SPI_SCK, uint8_t SPI_MOSI, uint8_t SPI_MISO, bool slowSPI>
void t_powerDownSPI()
    {
    ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
        {
        // Ensure that nSS is HIGH ASAP and thus any slave deselected when powering up SPI.
        fastDigitalWrite(SPI_nSS, HIGH);

        SPCR &= ~_BV(SPE); // Disable SPI.
        PRR |= _BV(PRSPI); // Power down...

        // Ensure that nSS is an output to avoid forcing SPI to slave mode by accident.
        pinMode(SPI_nSS, OUTPUT);

        // Avoid pins from floating when SPI is disabled.
        // Try to preserve general I/O direction and restore previous output values for outputs.
        pinMode(SPI_SCK, OUTPUT);
        pinMode(SPI_MOSI, OUTPUT);
        pinMode(SPI_MISO, INPUT_PULLUP);

        // If sharing SPI SCK with LED indicator then return this pin to being an output (retaining previous value).
        //if(LED_HEATCALL == PIN_SPI_SCK) { pinMode(LED_HEATCALL, OUTPUT); }
        }
    }

// STANDARD UNTEMPLATED DEFINITIONS OF SPI power up/down if PIN_SPI_nSS is defined.
// If SPI was disabled, power it up, enable it as master and with a sensible clock speed, etc, and return true.
// If already powered up then do nothing other than return false.
// If this returns true then a matching powerDownSPI() may be advisable.
inline bool powerUpSPIIfDisabled() { return(t_powerUpSPIIfDisabled<V0p2_PIN_SPI_nSS, DEFAULT_RUN_SPI_SLOW>()); }
// Power down SPI.
inline void powerDownSPI() { t_powerDownSPI<V0p2_PIN_SPI_nSS, V0p2_PIN_SPI_SCK, V0p2_PIN_SPI_MOSI, V0p2_PIN_SPI_MISO, DEFAULT_RUN_SPI_SLOW>(); }


/************** Serial IO stuff ************************/
// Moved from Power Management.h

// Check if serial is (already) powered up.
static inline bool _serialIsPoweredUp() { return(!(PRR & _BV(PRUSART0))); }


// If serial (UART/USART0) was disabled, power it up, do Serial.begin(), and return true.
// If already powered up then do nothing other than return false.
// If this returns true then a matching powerDownSerial() may be advisable.
// Defaults to V0p2 unit baud rate
template <uint16_t baud>
bool powerUpSerialIfDisabled()
  {
  if(_serialIsPoweredUp()) { return(false); }
  PRR &= ~_BV(PRUSART0); // Enable the UART.
  Serial.begin(baud); // Set it going.
  return(true);
  }
// Flush any pending serial (UART/USART0) output and power it down.
void powerDownSerial();
#ifdef __AVR_ATmega328P__
// Returns true if hardware USART0 buffer in ATMmega328P is non-empty; may occasionally return a spurious false.
// There may still be a byte in the process of being transmitted when this is false.
// This should not interfere with HardwareSerial's handling.
#define serialTXInProgress() (!(UCSR0A & _BV(UDRE0)))
// Does a Serial.flush() attempting to do some useful work (eg I/O polling) while waiting for output to drain.
// Assumes hundreds of CPU cycles available for each character queued for TX.
// Does not change CPU clock speed or disable or mess with USART0, though may poll it.
void flushSerialProductive();
// Does a Serial.flush() idling for 30ms at a time while waiting for output to drain.
// Does not change CPU clock speed or disable or mess with USART0, though may poll it.
// Sleeps in IDLE mode for up to 15ms at a time (using watchdog) waking early on interrupt
// so the caller must be sure RX overrun (etc) will not be an issue.
// Switches to flushSerialProductive() behaviour
// if in danger of overrunning a minor cycle while idling.
void flushSerialSCTSensitive();
#else
#define flushSerialProductive() Serial.flush()
#define flushSerialSCTSensitive() Serial.flush()
#endif


// If TWI (I2C) was disabled, power it up, do Wire.begin(), and return true.
// If already powered up then do nothing other than return false.
// If this returns true then a matching powerDownTWI() may be advisable.
bool powerUpTWIIfDisabled();
// Power down TWI (I2C).
void powerDownTWI();


// Enable power to intermittent peripherals.
//   * waitUntilStable  wait long enough (and maybe test) for I/O power to become stable.
// Waiting for stable may only be necessary for those items hung from IO_POWER cap;
// items powered direct from IO_POWER_UP may need no such wait.
void power_intermittent_peripherals_enable(bool waitUntilStable = false);

// Disable/remove power to intermittent peripherals.
void power_intermittent_peripherals_disable();


// Sensor for supply (eg battery) voltage in centivolts.
// Uses centivolts (cV) rather than millivolts (mv)
// to save transmitting/logging an information-free final digit
// even at the risk of some units confusion, though UCUM compliant.
// To use this an instance should be defined (there is no overhead if not).
class SupplyVoltageCentiVolts : public OTV0P2BASE::Sensor<uint16_t>
  {
  private:
    // Internal bandgap (1.1V nominal, 1.0--1.2V) as fraction of Vcc [0,1023].
    uint16_t rawInv;
    // Last measured supply voltage (cV) (nominally 0V--3.6V abs max) [0,360].
    uint16_t cV;
    // True if last-measured voltage was low.
    bool isLow;

  public:
    // Initialise to cautious values.
    SupplyVoltageCentiVolts() : cV(0), isLow(true) { }

    // Force a read/poll of the supply voltage and return the value sensed.
    // Expensive/slow.
    // NOT thread-safe or usable within ISRs (Interrupt Service Routines).
    virtual uint16_t read();

    // Return last value fetched by read(); undefined before first read()).
    // Fast.
    // NOT thread-safe nor usable within ISRs (Interrupt Service Routines).
    virtual uint16_t get() const { return(cV); }

    // Returns a suggested (JSON) tag/field/key name including units of get(); NULL means no recommended tag.
    // The lifetime of the pointed-to text must be at least that of the Sensor instance.
    virtual const char *tag() const { return("B|cV"); }

    // Get internal bandgap (1.1V nominal, 1.0--1.2V) as fraction of Vcc.
    uint16_t getRawInv() const { return(rawInv); }

    // Returns true if the supply voltage is low/marginal.
    // This depends on the AVR and other hardware components (eg sensors) in use.
    bool isSupplyVoltageLow() const { return(isLow); }

    // Returns true if the supply appears to be something that does not need monitoring.
    // This assumes that anything at/above 3V is mains
    // or at least a long way from needing monitoring.
    bool isMains() const { return(!isLow && (cV >= 300)); }
  };
//// Singleton implementation/instance.
//extern SupplyVoltageCentiVolts Supply_cV;


} // OTV0P2BASE
#endif // OTV0P2BASE_POWERMANAGEMENT_H
