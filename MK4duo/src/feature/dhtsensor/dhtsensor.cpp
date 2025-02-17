/**
 * MK4duo Firmware for 3D Printer, Laser and CNC
 *
 * Based on Marlin, Sprinter and grbl
 * Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm
 * Copyright (C) 2019 Alberto Cotronei @MagoKimbra
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * dhtsensor.cpp
 *
 * Copyright (C) 2019 Alberto Cotronei @MagoKimbra
 */

#include "../../../MK4duo.h"

#if ENABLED(DHT_SENSOR)

#define DHT_TIMEOUT -1

constexpr millis_s  DHTMinimumReadInterval = 2000, // ms
                    DHTMaximumReadTime     = 20;   // ms

DHTSensor dhtsensor;

/** Public Parameters */
dht_data_t DHTSensor::data;

float DHTSensor::Temperature  = 25,
      DHTSensor::Humidity     = 50;

/** Private Parameters */
uint8_t DHTSensor::read_data[5] = { 0, 0, 0, 0, 0 };
DHTSensor::SensorState DHTSensor::state = Init;

// ISR
uint16_t pulses[41];  // 1 start bit + 40 data bits
volatile uint16_t lastPulseTime;
volatile uint8_t numPulses;

void DHT_ISR() {
  const uint32_t now = micros();
  if (HAL::digitalRead(dhtsensor.data.pin) == HIGH)
    lastPulseTime = now;
  else if (lastPulseTime > 0) {
    pulses[numPulses++] = now - lastPulseTime;
    if (numPulses == COUNT(pulses))
      detachInterrupt(dhtsensor.data.pin);
  }
}

/** Public Function */
void DHTSensor::init() {
  HAL::pinMode(data.pin, OUTPUT);
  state = Init;
}

void DHTSensor::factory_parameters() {
  data.pin  = DHT_DATA_PIN;
  data.type = DHTEnum(DHT_TYPE);
}

void DHTSensor::change_type(const DHTEnum dhtType) {
  switch (dhtType) {
    case DHT11:
      data.type = DHT11;
      break;
    case DHT12:
      data.type = DHT12;
      break;
    case DHT21:
      data.type = DHT21;
      break;
    case DHT22:
      data.type = DHT22;
      break;
    default:
      SERIAL_LM(ER, "Invalid DHT sensor type");
      break;
  }
}

void DHTSensor::print_M305() {
  SERIAL_LM(CFG, "DHT sensor parameters: P<Pin> S<type 11-21-22>:");
  SERIAL_SM(CFG, "  M305 D0");
  SERIAL_MV(" P", data.pin);
  SERIAL_MV(" S", data.type);
  SERIAL_EOL();
}

void DHTSensor::spin() {

  static millis_s min_read_ms   = millis(),
                  operation_ms  = 0;

  if (min_read_ms && !expired(&min_read_ms, DHTMinimumReadInterval)) return;

  switch (state) {

    case Init:
      // Start the reading process
      HAL::digitalWrite(data.pin, INPUT_PULLUP);
      delay(1);

      // First set data line low for a period according to sensor type
      HAL::pinMode(data.pin, OUTPUT);
      HAL::digitalWrite(data.pin, LOW);
      switch (data.type) {
        case DHT22:
        case DHT21:
          HAL::delayMicroseconds(1100); // data sheet says "at least 1ms"
          break;
        default:
          delay(20); // data sheet says at least 18ms, 20ms just to be safe
          break;
      }

      // End the start signal by setting data line high for 40 microseconds.
      HAL::pinMode(data.pin, INPUT_PULLUP);

      HAL::delayMicroseconds(60); // Delay a bit to let sensor pull data line low.

      // Now start reading the data line to get the value from the DHT sensor.
      // Read from the DHT sensor using an DHT_ISR
      numPulses = COUNT(pulses);
      attachInterrupt(digitalPinToInterrupt(data.pin), DHT_ISR, CHANGE);
      lastPulseTime = 0;
      numPulses = 0;

      // Wait for the next operation to complete
      state = Read;
      min_read_ms = 0;
      operation_ms = millis();
      break;

    case Read:
      // Make sure we don't time out
      if (expired(&operation_ms, DHTMaximumReadTime)) {
        detachInterrupt(data.pin);
        state = Init;
        min_read_ms = millis();
        break;
      }

      // Wait for the reading to complete (1 start bit + 40 data bits)
      if (numPulses != COUNT(pulses)) break;

      // We're reading now - reset the state
      state = Init;
      min_read_ms = millis();

      // Check start bit
      if (pulses[0] < 40) break;

      // Reset 40 bits of received data to zero.
      ZERO(read_data);

      // Inspect each high pulse and determine which ones
      // are 0 (less than 40us) or 1 (more than 40us)
      for (uint8_t i = 0; i < 40; ++i) {
        read_data[i / 8] <<= 1;
        if (pulses[i + 1] > 40)
          read_data[i / 8] |= 1;
      }

      // Verify checksum
      if (((read_data[0] + read_data[1] + read_data[2] + read_data[3]) & 0xFF) != read_data[4])
        break;

      // Generate final results
      Temperature = read_temperature();
      Humidity    = read_humidity();

      break;
  }
}

float DHTSensor::dewPoint() {
  // (1) Saturation Vapor Pressure = ESGG(T)
  const float RATIO = 373.15 / (273.15 + Temperature);
  float RHS = -7.90298 * (RATIO - 1);
  RHS += 5.02808 * log10(RATIO);
  RHS += -1.3816e-7 * (POW(10, (11.344 * (1 - 1/RATIO ))) - 1) ;
  RHS += 8.1328e-3 * (POW(10, (-3.49149 * (RATIO - 1))) - 1) ;
  RHS += log10(1013.246);

  // factor -3 is to adjust units - Vapor Pressure SVP * Humidity
  const float VP = POW(10, RHS - 3) * Humidity;

  // (2) DEWPOINT = F(Vapor Pressure)
  float T = LOG(VP / 0.61078);   // temp var
  return (241.88 * T) / (17.558 - T);
}

float DHTSensor::dewPointFast() {
	const float a = 17.271f,
              b = 237.7f,
              temp = (a * Temperature) / (b + Temperature) + LOG(Humidity * 0.01f),
              Td = (b * temp) / (a - temp);
  return Td;
}

/** Private Function */
float DHTSensor::read_temperature() {
  float f = NAN;

  switch (data.type) {
    case DHT11:
      f = read_data[2];
      if (read_data[3] & 0x80) f = -1 - f;
      f += (read_data[3] & 0x0f) * 0.1;
      break;
    case DHT12:
      f = read_data[2] + (read_data[3] & 0x0f) * 0.1;
      if (read_data[2] & 0x80) f *= -1;
      break;
    case DHT21:
    case DHT22:
      f = (read_data[2] & 0x7F) << 8 | read_data[3];
      f *= 0.1;
      if (read_data[2] & 0x80) f *= -1;
      break;
    default: break;
  }
  return f;
}

float DHTSensor::read_humidity() {
  float f = NAN;

  switch (data.type) {
    case DHT11:
    case DHT12:
      f = read_data[0] + read_data[1] * 0.1;
      break;
    case DHT21:
    case DHT22:
      f = read_data[0] << 8 | read_data[1];
      f *= 0.1;
      break;
    default: break;
  }
  return f;
}

#endif // ENABLED(DHT_SENSOR)
