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

//
// Service Menus
//

#include "../../../MK4duo.h"

#if HAS_LCD_MENU && HAS_SERVICE_TIMES

#include "menu.h"

inline void _lcd_reset_service(const int index) {
  print_job_counter.resetServiceTime(index);
  #if HAS_BUZZER
    sound.playtone(200, 404);
  #endif
  lcdui.reset_status();
  lcdui.return_to_status();
}

#if ENABLED(SERVICE_TIME_1)
  void menu_action_reset_service1() { _lcd_reset_service(1); }
#endif

#if ENABLED(SERVICE_TIME_2)
  void menu_action_reset_service2() { _lcd_reset_service(2); }
#endif

#if ENABLED(SERVICE_TIME_3)
  void menu_action_reset_service3() { _lcd_reset_service(3); }
#endif

inline void _menu_service(const int index) {
  START_MENU();
  MENU_BACK(MSG_MAIN);
  switch (index) {
    #if ENABLED(SERVICE_TIME_1)
      case 1: MENU_ITEM(function, MSG_SERVICE_RESET, menu_action_reset_service1); break;
    #endif
    #if ENABLED(SERVICE_TIME_2)
      case 2: MENU_ITEM(function, MSG_SERVICE_RESET, menu_action_reset_service2); break;
    #endif
    #if ENABLED(SERVICE_TIME_3)
      case 3: MENU_ITEM(function, MSG_SERVICE_RESET, menu_action_reset_service3); break;
    #endif
  }
  END_MENU();
}

#if ENABLED(SERVICE_TIME_1)
  void menu_service1() { _menu_service(1); }
#endif

#if ENABLED(SERVICE_TIME_2)
  void menu_service2() { _menu_service(2); }
#endif

#if ENABLED(SERVICE_TIME_3)
  void menu_service3() { _menu_service(3); }
#endif

#endif // HAS_LCD_MENU && HAS_SERVICE_TIMES
