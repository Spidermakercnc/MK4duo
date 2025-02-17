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

#if HEATER_COUNT > 0

#define CODE_M116

/**
 * M116: Wait for all heaters to reach target temperature
 */
inline void gcode_M116(void) {
  #if HOTENDS > 0
    LOOP_HOTEND() hotends[h].wait_for_target(true);
  #endif
  #if BEDS > 0
    LOOP_BED() beds[h].wait_for_target(true);
  #endif
  #if CHAMBERS > 0
    LOOP_CHAMBER() chambers[h].wait_for_target(true);
  #endif
}

#endif
