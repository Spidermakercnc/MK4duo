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
#pragma once

/**
 * nextion_lcd.h
 *
 * Copyright (c) 2019 Alberto Cotronei @MagoKimbra
 *
 * Grbl is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Grbl is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Grbl. If not, see <http://www.gnu.org/licenses/>.
 */

// For debug Connect
//#define NEXTION_CONNECT_DEBUG

#if HAS_NEXTION_LCD

#define NEXTION_BUFFER_SIZE  50
#define LCD_UPDATE_INTERVAL 300U

#if HAS_LCD_MENU

  void _wrap_string(uint8_t &y, const char * const string, read_byte_cb_t cb_read_byte, const bool wordwrap=false);
  inline void wrap_string_P(uint8_t &y, PGM_P const pstr, const bool wordwrap=false)        { _wrap_string(y, pstr,    read_byte_rom,  wordwrap);  }
  inline void wrap_string(uint8_t &y, const char * const string, const bool wordwrap=false) { _wrap_string(y, string,  read_byte_ram,  wordwrap);  }

#endif // HAS_LCD_MENU

void nextion_draw_update();
void lcd_scrollinfo(PGM_P titolo, PGM_P message);

#if ENABLED(NEXTION_GFX)
  void gfx_origin(const float x, const float y, const float z);
  void gfx_scale(const float scale);
  void gfx_clear(const float x, const float y, const float z);
  void gfx_cursor_to(const float x, const float y, const float z, bool force_cursor=false);
  void gfx_line_to(const float x, const float y, const float z);
  void gfx_plane_to(const float x, const float y, const float z);
#endif

#if HAS_SD_SUPPORT
  void UploadNewFirmware();
#endif

#if ENABLED(RFID_MODULE)
  void rfid_setText(PGM_P message, uint32_t color=65535);
#endif

#endif // HAS_NEXTION_LCD
