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
 * mcode
 *
 * Copyright (C) 2019 Alberto Cotronei @MagoKimbra
 */

#if BEDS > 0

#define CODE_M190

/**
 * M190: Sxxx Wait for bed current temp to reach target temp. Waits only when heating
 *       Rxxx Wait for bed current temp to reach target temp. Waits when heating and cooling
 */
inline void gcode_M190(void) {
  if (printer.debugDryrun() || printer.debugSimulation()) return;

  const uint8_t b = parser.byteval('T');
  if (WITHIN(b, 0 , BEDS - 1)) {
    const bool no_wait_for_cooling = parser.seen('S');
    if (no_wait_for_cooling || parser.seen('R'))
      beds[b].setTarget(parser.value_celsius());
    else return;

    lcdui.set_status_P(beds[b].isHeating() ? PSTR(MSG_BED_HEATING) : PSTR(MSG_BED_COOLING));

    beds[b].wait_for_target(no_wait_for_cooling);
  }
}

#endif // BEDS > 0
