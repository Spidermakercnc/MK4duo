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

#include "../../../MK4duo.h"

#if HAS_LCD_MENU

////////////////////////////////////////////
///////////// Global Variables /////////////
////////////////////////////////////////////

// Manual Movement
float move_menu_scale;

// Menu Navigation
int8_t encoderTopLine, encoderLine, screen_items;

typedef struct {
  screenFunc_t menu_function;
  uint32_t  encoder_position;
  int8_t    top_line, items;
} menuPosition;
menuPosition screen_history[6];
uint8_t screen_history_depth = 0;
bool screen_changed;

// Value Editing
PGM_P MenuItemBase::editLabel;
void* MenuItemBase::editValue;
int32_t MenuItemBase::minEditValue, MenuItemBase::maxEditValue;
screenFunc_t MenuItemBase::callbackFunc;
bool MenuItemBase::liveEdit;

// Prevent recursion into screen handlers
bool no_reentry = false;

////////////////////////////////////////////
//////// Menu Navigation & History /////////
////////////////////////////////////////////

void LcdUI::return_to_status() { goto_screen(status_screen); }

void LcdUI::save_previous_screen() {
  if (screen_history_depth < COUNT(screen_history))
    screen_history[screen_history_depth++] = { currentScreen, encoderPosition, encoderTopLine, screen_items };
}

void LcdUI::goto_previous_screen() {
  if (screen_history_depth > 0) {
    menuPosition &sh = screen_history[--screen_history_depth];
    goto_screen(sh.menu_function, sh.encoder_position, sh.top_line, sh.items);
  }
  else
    return_to_status();
}

////////////////////////////////////////////
/////////// Common Menu Actions ////////////
////////////////////////////////////////////

void MenuItem_gcode::action(PGM_P pgcode) { commands.inject_P(pgcode); }

////////////////////////////////////////////
/////////// Menu Editing Actions ///////////
////////////////////////////////////////////

/**
 * Functions for editing single values
 *
 * The "DEFINE_MENU_EDIT_ITEM" macro generates the functions needed to edit a numerical value.
 *
 * The prerequisite is that in the header the type was already declared:
 *
 *   DECLARE_MENU_EDIT_TYPE(int16_t, int3, i16tostr3, 1)
 *
 * For example, DEFINE_MENU_EDIT_ITEM(int3) expands into these functions:
 *
 *   bool MenuItem_int3::_edit();
 *   void MenuItem_int3::edit(); // edit int16_t (interactively)
 *   void MenuItem_int3::action_edit(PGM_P const pstr, int16_t * const ptr, const int16_t minValue, const int16_t maxValue, const screenFunc_t callback = null, const bool live = false);
 *
 * You can then use one of the menu macros to present the edit interface:
 *   MENU_ITEM_EDIT(int3, MSG_SPEED, &feedrate_percentage, 10, 999)
 *
 * This expands into a more primitive menu item:
 *   MENU_ITEM_VARIANT(int3, _edit, MSG_SPEED, PSTR(MSG_SPEED), &feedrate_percentage, 10, 999)
 *
 * ...which calls:
 *       MenuItem_int3::action_edit(PSTR(MSG_SPEED), &feedrate_percentage, 10, 999)
 */
void MenuItemBase::edit(strfunc_t strfunc, loadfunc_t loadfunc) {
  lcdui.encoder_direction_normal();
  if ((int16_t)lcdui.encoderPosition < 0) lcdui.encoderPosition = 0;
  if ((int16_t)lcdui.encoderPosition > maxEditValue) lcdui.encoderPosition = maxEditValue;
  if (lcdui.should_draw())
    draw_edit_screen(editLabel, strfunc(lcdui.encoderPosition + minEditValue));
  if (lcdui.lcd_clicked || (liveEdit && lcdui.should_draw())) {
    if (editValue != nullptr) loadfunc(editValue, lcdui.encoderPosition + minEditValue);
    if (callbackFunc && (liveEdit || lcdui.lcd_clicked)) (*callbackFunc)();
    if (lcdui.use_click()) lcdui.goto_previous_screen();
  }
}

void MenuItemBase::init(PGM_P const el, void * const ev, const int32_t minv, const int32_t maxv, const uint16_t ep, const screenFunc_t cs, const screenFunc_t cb, const bool le) {
  lcdui.save_previous_screen();
  lcdui.refresh();
  editLabel = el;
  editValue = ev;
  minEditValue = minv;
  maxEditValue = maxv;
  lcdui.encoderPosition = ep;
  lcdui.currentScreen = cs;
  callbackFunc = cb;
  liveEdit = le;
}

#define DEFINE_MENU_EDIT_ITEM(NAME) template class TMenuItem<MenuItemInfo_##NAME>

DEFINE_MENU_EDIT_ITEM(int3);
DEFINE_MENU_EDIT_ITEM(int4);
DEFINE_MENU_EDIT_ITEM(int8);
DEFINE_MENU_EDIT_ITEM(uint8);
DEFINE_MENU_EDIT_ITEM(microstep);
DEFINE_MENU_EDIT_ITEM(uint16_3);
DEFINE_MENU_EDIT_ITEM(uint16_4);
DEFINE_MENU_EDIT_ITEM(float3);
DEFINE_MENU_EDIT_ITEM(float52);
DEFINE_MENU_EDIT_ITEM(float43);
DEFINE_MENU_EDIT_ITEM(float5);
DEFINE_MENU_EDIT_ITEM(float5_25);
DEFINE_MENU_EDIT_ITEM(float51);
DEFINE_MENU_EDIT_ITEM(float51sign);
DEFINE_MENU_EDIT_ITEM(float52sign);
DEFINE_MENU_EDIT_ITEM(long5);
DEFINE_MENU_EDIT_ITEM(long5_25);

void MenuItem_bool::action_edit(PGM_P pstr, bool *ptr, screenFunc_t callback) {
  UNUSED(pstr); *ptr ^= true; lcdui.refresh();
  if (callback) (*callback)();
}

////////////////////////////////////////////
///////////////// Menu Tree ////////////////
////////////////////////////////////////////

#if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
  float lcd_z_fade_height;
  void lcd_set_z_fade_height() { bedlevel.set_z_fade_height(lcd_z_fade_height); }
#endif

/**
 * General function to go directly to a screen
 */
void LcdUI::goto_screen(screenFunc_t screen, const uint16_t encoder/*=0*/, const int8_t top/*=0*/, const int8_t items/*=0*/) {
  if (currentScreen != screen) {

    #if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
      // Shadow for editing the fade height
      lcd_z_fade_height = bedlevel.z_fade_height;
    #endif

    #if ENABLED(DOUBLECLICK_FOR_Z_BABYSTEPPING) && ENABLED(BABYSTEPPING)
      static millis_s doubleclick_expire_ms = millis();
      // Going to menu_main from status screen? Remember first click time.
      // Going back to status screen within a very short time? Go to Z babystepping.
      if (screen == menu_main) {
        if (on_status_screen())
          doubleclick_expire_ms = millis();
      }
      else if (screen == status_screen && currentScreen == menu_main && pending(&doubleclick_expire_ms, DOUBLECLICK_MAX_INTERVAL)) {
        screen =
          #if ENABLED(BABYSTEP_ZPROBE_OFFSET)
            lcd_babystep_zoffset
          #else
            lcd_babystep_z
          #endif
        ;
      }
    #endif

    currentScreen = screen;
    encoderPosition = encoder;
    encoderTopLine = top;
    screen_items = items;
    if (screen == status_screen) {
      lcdui.defer_status_screen(false);
      #if ENABLED(AUTO_BED_LEVELING_UBL)
        ubl.lcd_map_control = false;
      #endif
      screen_history_depth = 0;
    }

    clear_lcd();

    // Re-initialize custom characters that may be re-used
    #if HAS_CHARACTER_LCD
      #if ENABLED(AUTO_BED_LEVELING_UBL)
        if (!ubl.lcd_map_control)
      #endif
          set_custom_characters(screen == status_screen ? CHARSET_INFO : CHARSET_MENU);
    #endif

    refresh(LCDVIEW_CALL_REDRAW_NEXT);
    screen_changed = true;
    #if HAS_GRAPHICAL_LCD
      drawing_screen = false;
    #endif

    set_lcdui_selection(false);
  }
}

////////////////////////////////////////////
///////////// Manual Movement //////////////
////////////////////////////////////////////

//
// Display the synchronize screen until moves are
// finished, and don't return to the caller until
// done. ** This blocks the command queue! **
//
static PGM_P sync_message;

void LcdUI::_synchronize() {
  if (should_draw()) draw_menu_item_static(LCD_HEIGHT >= 4 ? 1 : 0, sync_message);
  if (no_reentry) return;
  // Make this the current handler till all moves are done
  no_reentry = true;
  const screenFunc_t old_screen = currentScreen;
  goto_screen(_synchronize);
  planner.synchronize(); // idle() is called until moves complete
  no_reentry = false;
  goto_screen(old_screen);
}

// Display the synchronize screen with a custom message
// ** This blocks the command queue! **
void LcdUI::synchronize(PGM_P const msg/*=nullptr*/) {
  static const char moving[] PROGMEM = MSG_MOVING;
  sync_message = msg ? msg : moving;
  _synchronize();
}

/**
 * Scrolling for menus and other line-based screens
 *
 *   encoderLine is the position based on the encoder
 *   encoderTopLine is the top menu line to display
 *   _lcdLineNr is the index of the LCD line (e.g., 0-3)
 *   _menuLineNr is the menu item to draw and process
 *   _thisItemNr is the index of each MENU_ITEM or STATIC_ITEM
 *   screen_items is the total number of items in the menu (after one call)
 */
void scroll_screen(const uint8_t limit, const bool is_menu) {
  lcdui.encoder_direction_menus();
  ENCODER_RATE_MULTIPLY(false);
  if (lcdui.encoderPosition > 0x8000) lcdui.encoderPosition = 0;
  if (lcdui.first_page) {
    encoderLine = lcdui.encoderPosition / (ENCODER_STEPS_PER_MENU_ITEM);
    screen_changed = false;
  }
  if (screen_items > 0 && encoderLine >= screen_items - limit) {
    encoderLine = MAX(0, screen_items - limit);
    lcdui.encoderPosition = encoderLine * (ENCODER_STEPS_PER_MENU_ITEM);
  }
  if (is_menu) {
    NOMORE(encoderTopLine, encoderLine);
    if (encoderLine >= encoderTopLine + LCD_HEIGHT)
      encoderTopLine = encoderLine - LCD_HEIGHT + 1;
  }
  else
    encoderTopLine = encoderLine;
}

#if HAS_LINE_TO_Z

  void line_to_z(const float &z) {
    mechanics.current_position[Z_AXIS] = z;
    planner.buffer_line(mechanics.current_position, MMM_TO_MMS(manual_feedrate_mm_m[Z_AXIS]), tools.active_extruder);
  }

#endif

#if ENABLED(BABYSTEP_ZPROBE_OFFSET)

  void lcd_babystep_zoffset() {
    if (lcdui.use_click()) return lcdui.goto_previous_screen_no_defer();
    lcdui.defer_status_screen();
    #if ENABLED(BABYSTEP_HOTEND_Z_OFFSET)
      const bool do_probe = (tools.active_extruder == 0);
    #else
      constexpr bool do_probe = true;
    #endif
    lcdui.encoder_direction_normal();
    if (lcdui.encoderPosition) {
      const int16_t babystep_increment = (int16_t)lcdui.encoderPosition * (BABYSTEP_MULTIPLICATOR);
      lcdui.encoderPosition = 0;

      const float diff = mechanics.steps_to_mm[Z_AXIS] * babystep_increment,
                  new_probe_offset = probe.data.offset[Z_AXIS] + diff,
                  new_offs =
                    #if ENABLED(BABYSTEP_HOTEND_Z_OFFSET)
                      do_probe ? new_probe_offset : tools.data.hotend_offset[Z_AXIS][tools.active_extruder] - diff
                    #else
                      new_probe_offset
                    #endif
                  ;
      if (WITHIN(new_offs, Z_PROBE_OFFSET_RANGE_MIN, Z_PROBE_OFFSET_RANGE_MAX)) {

        babystep.add_steps(Z_AXIS, babystep_increment);

        if (do_probe) probe.data.offset[Z_AXIS] = new_offs;
        #if ENABLED(BABYSTEP_HOTEND_Z_OFFSET)
          else tools.data.hotend_offset[Z_AXIS][tools.active_extruder] = new_offs;
        #endif

        lcdui.refresh(LCDVIEW_CALL_REDRAW_NEXT);
      }
    }
    if (lcdui.should_draw()) {
      #if ENABLED(BABYSTEP_HOTEND_Z_OFFSET)
        if (!do_probe)
          draw_edit_screen(PSTR(MSG_DXC_Z_OFFSET), ftostr43sign(tools.data.hotend_offset[Z_AXIS][tools.active_extruder]));
        else
      #endif
          draw_edit_screen(PSTR(MSG_ZPROBE_ZOFFSET), ftostr43sign(probe.data.offset[Z_AXIS]));

      #if ENABLED(BABYSTEP_ZPROBE_GFX_OVERLAY)
        if (do_probe) _lcd_zoffset_overlay_gfx(probe.data.offset[Z_AXIS]);
      #endif
    }
  }

#endif // BABYSTEP_ZPROBE_OFFSET

/**
 * Watch temperature callbacks
 */
#if HOTENDS > 0

  void watch_temp_callback_H0() {
    hotends[0].setTarget(hotends[0].target_temperature);
    hotends[0].start_watching();
  }
  #if HOTENDS > 1
    void watch_temp_callback_H1() {
      hotends[1].setTarget(hotends[1].target_temperature);
      hotends[1].start_watching();
    }
    #if HOTENDS > 2
      void watch_temp_callback_H2() {
        hotends[2].setTarget(hotends[2].target_temperature);
        hotends[2].start_watching();
      }
      #if HOTENDS > 3
        void watch_temp_callback_H3() {
          hotends[3].setTarget(hotends[3].target_temperature);
          hotends[3].start_watching();
        }
        #if HOTENDS > 4
          void watch_temp_callback_H4() {
            hotends[4].setTarget(hotends[4].target_temperature);
            hotends[4].start_watching();
          }
          #if HOTENDS > 3
            void watch_temp_callback_H5() {
              hotends[5].setTarget(hotends[5].target_temperature);
              hotends[5].start_watching();
            }
          #endif // HOTENDS > 5
        #endif // HOTENDS > 4
      #endif // HOTENDS > 3
    #endif // HOTENDS > 2
  #endif // HOTENDS > 1

#endif // HAS_TEMP_HOTEND

#if BEDS > 0
  void watch_temp_callback_bed0() {
    beds[0].setTarget(beds[0].target_temperature);
    beds[0].start_watching();
  }
  #if BEDS > 1
    void watch_temp_callback_bed1() {
      beds[1].setTarget(beds[1].target_temperature);
      beds[1].start_watching();
    }
    #if BEDS > 2
      void watch_temp_callback_bed2() {
        beds[2].setTarget(beds[2].target_temperature);
        beds[2].start_watching();
      }
      #if BEDS > 3
        void watch_temp_callback_bed3() {
          beds[3].setTarget(beds[3].target_temperature);
          beds[3].start_watching();
        }
      #endif // BEDS > 3
    #endif // BEDS > 2
  #endif // BEDS > 1
#endif // BEDS > 0

#if CHAMBERS > 0
  void watch_temp_callback_chamber0() {
    chambers[0].setTarget(chambers[0].target_temperature);
    chambers[0].start_watching();
  }
  #if CHAMBERS > 1
    void watch_temp_callback_chamber1() {
      chambers[1].setTarget(chambers[1].target_temperature);
      chambers[1].start_watching();
    }
    #if CHAMBERS > 2
      void watch_temp_callback_chamber2() {
        chambers[2].setTarget(chambers[2].target_temperature);
        chambers[2].start_watching();
      }
      #if CHAMBERS > 3
        void watch_temp_callback_chamber3() {
          chambers[3].setTarget(chambers[3].target_temperature);
          chambers[3].start_watching();
        }
      #endif // CHAMBERS > 3
    #endif // CHAMBERS > 2
  #endif // CHAMBERS > 1
#endif // CHAMBERS > 0

#if COOLERS > 0
  void watch_temp_callback_cooler0() {
    coolers[0].setTarget(coolers[0].target_temperature);
    coolers[0].start_watching();
  }
#endif

#if ENABLED(AUTO_BED_LEVELING_UBL) || ENABLED(PID_AUTOTUNE_MENU) || ENABLED(ADVANCED_PAUSE_FEATURE)

  void lcd_enqueue_one_now(const char * const cmd) {
    no_reentry = true;
    commands.enqueue_one_now(cmd);
    no_reentry = false;
  }

  void lcd_enqueue_one_now_P(PGM_P const cmd) {
    no_reentry = true;
    commands.enqueue_now_P(cmd);
    no_reentry = false;
  }

#endif

#if ENABLED(EEPROM_SETTINGS)
  void lcd_store_settings()   { eeprom.store(); }
  void lcd_load_settings()    { eeprom.load(); }
#endif

void lcd_draw_homing() {
  constexpr uint8_t line = (LCD_HEIGHT - 1) / 2;
  if (lcdui.should_draw()) draw_menu_item_static(line, PSTR(MSG_LEVEL_BED_HOMING));
  lcdui.refresh(LCDVIEW_CALL_NO_REDRAW);
}

//
// Selection screen presents a prompt and two options
//
bool lcdui_selection; // = false
void set_lcdui_selection(const bool sel) { lcdui_selection = sel; }
void do_select_screen(PGM_P const yes, PGM_P const no, selectFunc_t yesFunc, selectFunc_t noFunc, PGM_P const pref, const char * const string/*=nullptr*/, PGM_P const suff/*=nullptr*/) {
  if (lcdui.encoderPosition) {
    lcdui_selection = int16_t(lcdui.encoderPosition) > 0;
    lcdui.encoderPosition = 0;
  }
  const bool got_click = lcdui.use_click();
  if (got_click || lcdui.should_draw()) {
    draw_select_screen(yes, no, lcdui_selection, pref, string, suff);
    if (got_click) { lcdui_selection ? yesFunc() : noFunc(); }
  }
}

#if ENABLED(LCD_BED_LEVELING) || (HAS_LEVELING && DISABLED(SLIM_LCD_MENUS))
  void lcd_toggle_bed_leveling() { bedlevel.set_bed_leveling_enabled(!bedlevel.flag.leveling_active); }
#endif

#if HAS_SOFTWARE_ENDSTOPS
  void lcd_toggle_soft_endstops() { endstops.setSoftEndstop(!endstops.flag.SoftEndstop); }
#endif

#endif // HAS_LCD_MENU
