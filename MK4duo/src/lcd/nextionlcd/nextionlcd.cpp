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
 * nextionlcd.cpp
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

#include "../../../MK4duo.h"

#if HAS_NEXTION_LCD

#define NEXTION_LCD_FIRMWARE_VERSION  114

#include "library/nextion.h"
#include "nextion_gfx.h"

LcdUI       lcdui;

char        LcdUI::status_message[NEXTION_MAX_MESSAGE_LENGTH + 1] = WELCOME_MSG;
uint8_t     LcdUI::status_message_level; // = 0

#if HAS_LCD_MENU

  LCDViewActionEnum LcdUI::lcdDrawUpdate = LCDVIEW_CLEAR_CALL_REDRAW;

  uint16_t    LcdUI::encoderPosition;

  screenFunc_t  LcdUI::currentScreen;

  #if HAS_SD_SUPPORT && ENABLED(SCROLL_LONG_FILENAMES)
    uint8_t LcdUI::filename_scroll_pos, LcdUI::filename_scroll_max;
  #endif

  #if ENABLED(REVERSE_MENU_DIRECTION)
    int8_t LcdUI::encoderDirection = 1;
  #endif

  bool          LcdUI::lcd_clicked = false,
                line_encoder_touch = true;

  #if LCD_TIMEOUT_TO_STATUS
    bool      LcdUI::defer_return_to_status;
    millis_l  return_to_status_ms = 0;
  #endif

  extern bool no_reentry; // Flag to prevent recursion into menu handlers

  int8_t manual_move_axis = (int8_t)NO_AXIS;
  millis_s manual_move_ms = 0;

  #if IS_KINEMATIC
    bool LcdUI::processing_manual_move = false;
    float manual_move_offset = 0;
  #endif

  #if E_MANUAL > 1
    int8_t LcdUI::manual_move_e_index = 0;
  #endif

  #if ENABLED(AUTO_BED_LEVELING_UBL) || ENABLED(G26_MESH_VALIDATION)
    bool LcdUI::external_control; // = false
  #endif

#endif

bool        NextionON                   = false;
uint8_t     PageID                      = 0;
char        buffer[NEXTION_BUFFER_SIZE] = { 0 };

// 0 card not present, 1 SD not insert, 2 SD insert, 3 SD_HOST printing, 4 SD_HOST paused
enum SDstatus_enum {NO_SD = 0, SD_NO_INSERT = 1, SD_INSERT = 2, SD_HOST_PRINTING = 3, SD_HOST_PAUSE = 4 };
SDstatus_enum SDstatus    = NO_SD;

#if HAS_SD_SUPPORT
  #if PIN_EXISTS(SD_DETECT)
    uint8_t lcd_sd_status = 2; // UNKNOWN
  #endif
  NexUpload Firmware(NEXTION_FIRMWARE_FILE, 57600);
#endif

#if ENABLED(NEXTION_GFX)
  GFX gfx = GFX(1, 1, 1, 1);
#endif

/**
 *******************************************************************
 * Nextion component all page
 *******************************************************************
 */
NexObject Pstart        = NexObject(0,  0);
NexObject Pmenu         = NexObject(1,  0);
NexObject Pprinter      = NexObject(2,  0);
NexObject Psetup        = NexObject(3,  0);
NexObject Pmove         = NexObject(4,  0);
NexObject Pspeed        = NexObject(5,  0);
NexObject Pgcode        = NexObject(6,  0);
NexObject Prfid         = NexObject(7,  0);
NexObject Pbrightness   = NexObject(8,  0);
NexObject Ptemp         = NexObject(9,  0);
NexObject Pfilament     = NexObject(10, 0);
NexObject Ptxtmenu      = NexObject(11, 0);

/**
 *******************************************************************
 * Nextion component for page:menu
 *******************************************************************
 */
NexObject SDMenu      = NexObject(1,  2);

/**
 *******************************************************************
 * Nextion component for page:printer
 *******************************************************************
 */
NexObject LcdX        = NexObject(2,  5);
NexObject LcdY        = NexObject(2,  6);
NexObject LcdZ        = NexObject(2,  7);
NexObject Hotend00    = NexObject(2,  9);
NexObject Hotend01    = NexObject(2, 10);
NexObject Hotend10    = NexObject(2, 11);
NexObject Hotend11    = NexObject(2, 12);
NexObject Bed0        = NexObject(2, 13);
NexObject Bed1        = NexObject(2, 14);
NexObject Chamber0    = NexObject(2, 15);
NexObject Chamber1    = NexObject(2, 16);
NexObject DHT0        = NexObject(2, 17);
NexObject SD          = NexObject(2, 18);
NexObject Fanspeed    = NexObject(2, 21);
NexObject VSpeed      = NexObject(2, 22);
NexObject LightStatus = NexObject(2, 24);
NexObject NStop       = NexObject(2, 35);
NexObject NPlay       = NexObject(2, 36);
NexObject Light       = NexObject(2, 37);
NexObject LcdStatus   = NexObject(2, 92);
NexObject LcdCommand  = NexObject(2, 93);
NexObject LcdTime     = NexObject(2, 94);
NexObject progressbar = NexObject(2, 95);
NexObject FanTouch    = NexObject(2, 100);

/**
 *******************************************************************
 * Nextion component for page:Setup
 *******************************************************************
 */
#if HAS_SD_SUPPORT
  NexObject NextionFW = NexObject(3,  1);
#endif
#if HAS_LCD_MENU
  NexObject TxtMenu   = NexObject(3,  3);
#endif

/**
 *******************************************************************
 * Nextion component for page:Move
 *******************************************************************
 */
NexObject XYHome      = NexObject(4,   2);
NexObject XYUp        = NexObject(4,   3);
NexObject XYRight     = NexObject(4,   4);
NexObject XYDown      = NexObject(4,   5);
NexObject XYLeft      = NexObject(4,   6);
NexObject ZHome       = NexObject(4,   7);
NexObject ZUp         = NexObject(4,   8);
NexObject ZDown       = NexObject(4,   9);
NexObject movecmd     = NexObject(4,  11);
NexObject LedCoord5   = NexObject(4,  12);
NexObject MotorOff    = NexObject(4,  17);
NexObject ext         = NexObject(4,  18);
NexObject Extrude     = NexObject(4,  19);
NexObject Retract     = NexObject(4,  21);
NexObject SpeedX      = NexObject(4,  22);
NexObject SpeedY      = NexObject(4,  23);
NexObject SpeedZ      = NexObject(4,  24);
NexObject SpeedE      = NexObject(4,  25);

/**
 *******************************************************************
 * Nextion component for page:GCode
 *******************************************************************
 */
NexObject Tgcode      = NexObject(6,   1);
NexObject Send        = NexObject(6,  27);

#if ENABLED(RFID_MODULE)

  /**
   *******************************************************************
   * Nextion component for page:Rfid
   *******************************************************************
   */
  NexObject Rfid0       = NexObject(7,  2);
  NexObject Rfid1       = NexObject(7,  3);
  NexObject Rfid2       = NexObject(7,  4);
  NexObject Rfid3       = NexObject(7,  5);
  NexObject Rfid4       = NexObject(7,  6);
  NexObject Rfid5       = NexObject(7,  7);
  NexObject RfidText    = NexObject(7,  8);
  NexObject RfidR       = NexObject(7,  9);

#endif

/**
 *******************************************************************
 * Nextion component for page:Temp
 *******************************************************************
 */
NexObject Theater     = NexObject(9,   1);
NexObject Tenter      = NexObject(9,   2);
NexObject Tset        = NexObject(9,  15);

/**
 *******************************************************************
 * Nextion component for page:Filament
 *******************************************************************
 */
NexObject FilLoad     = NexObject(10,  3);
NexObject FilUnload   = NexObject(10,  4);
NexObject FilExtr     = NexObject(10,  5);
NexObject Filgcode    = NexObject(10, 10);

/**
 *******************************************************************
 * Nextion component for page:Select
 *******************************************************************
 */
NexObject EncRow1     = NexObject(11,  1);
NexObject EncRow2     = NexObject(11,  2);
NexObject EncRow3     = NexObject(11,  3);
NexObject EncRow4     = NexObject(11,  4);
NexObject EncRow5     = NexObject(11,  5);
NexObject EncRow6     = NexObject(11,  6);
NexObject EncUp       = NexObject(11,  7);
NexObject EncSend     = NexObject(11,  8);
NexObject EncDown     = NexObject(11,  9);
NexObject EncExit     = NexObject(11, 10);

NexObject *nex_listen_list[] =
{
  // Page 1 touch listen
  &SDMenu,

  // Page 2 touch listen
  &FanTouch, &NStop, &NPlay, &Light,

  // Page 3 touch listen
  #if HAS_SD_SUPPORT
    &NextionFW,
  #endif
  #if HAS_LCD_MENU
    &TxtMenu,
  #endif

  // Page 4 touch listen
  &MotorOff, &XYHome, &XYUp, &XYRight, &XYDown, &XYLeft,
  &ZHome, &ZUp, &ZDown,
  &Extrude, &Retract,

  // Page 6 touch listen
  &Send,

  #if ENABLED(RFID_MODULE)
    // Page 7 touch listen
    &Rfid0, &Rfid1, &Rfid2, &Rfid3, &Rfid4, &Rfid5,
  #endif

  // Page 8 touch listen
  &Tenter,

  // Page 11 touch listen
  &FilLoad, &FilUnload, &FilExtr,

  NULL
};

NexObject *heater_list0[] =
{
  &Hotend00,
  &Hotend10,
  &Bed0,
  &Chamber0,
  &DHT0,
  NULL
};

NexObject *heater_list1[] =
{
  &Hotend01,
  &Hotend11,
  &Bed1,
  &Chamber1,
  NULL
};

NexObject *speed_list[] =
{
  &SpeedX,
  &SpeedY,
  &SpeedZ,
  &SpeedE,
  NULL
};

void setpagePrinter() {
  char temp[10] = { 0 };

  nexlcd.sendCommandPGM(PSTR("p[1].b[10].txt=\"" SHORT_BUILD_VERSION "\""));

  #if HOTENDS > 0
    nexlcd.setValue(Hotend00, 1);
    #if HOTENDS > 1
      nexlcd.setValue(Hotend10, 1);
    #elif HAS_TEMP_CHAMBER0
      nexlcd.setValue(Chamber0, 1);
    #elif ENABLED(DHT_SENSOR)
      nexlcd.setValue(DHT0, 1);
    #endif
  #endif

  #if BEDS > 0
    nexlcd.setValue(Bed0, 1);
  #endif

  #define EXTRUDERS_STRING(M) STRINGIFY(M)
  #define NEXTION_EXTRUDERS EXTRUDERS_STRING(EXTRUDERS)
  nexlcd.sendCommandPGM(PSTR("p[2].b[8].val=" NEXTION_EXTRUDERS));

  LOOP_XYZE(i) {
    ZERO(temp);
    itoa(manual_feedrate_mm_m[i], temp, 10);
    nexlcd.setText(*speed_list[i], temp);
  }

  #if HAS_SD_SUPPORT
    if (!card.isDetected()) card.mount();
    HAL::delayMilliseconds(500);
    if (card.isDetected()) {
      SDstatus = SD_INSERT;
      card.beginautostart();  // Initial boot
    }
    else
      SDstatus = SD_NO_INSERT;
    nexlcd.setValue(SD, SDstatus);
  #endif

  nexlcd.setValue(VSpeed, 100);

  #if FAN_COUNT > 0
    nexlcd.sendCommandPGM(PSTR("p[2].b[20].val=1"));
  #endif

  #if HAS_CASE_LIGHT
    nexlcd.setValue(LightStatus, caselight.status ? 2 : 1);
  #endif

  #if ENABLED(RFID_MODULE)
    nexlcd.sendCommandPGM(PSTR("p[2].b[19].val=1"));
  #endif

  #define LANGUAGE_STRING(M) STRINGIFY(M)
  #define NEXTION_LANGUAGE LANGUAGE_STRING(LCD_LANGUAGE)
  nexlcd.sendCommandPGM(PSTR("p[2].b[23].txt=\"" NEXTION_LANGUAGE "\""));

}

#if HAS_SD_SUPPORT

  void UploadNewFirmware() {
    if (IS_SD_INSERTED() || card.isDetected()) {
      Firmware.startUpload();
      nexSerial.end();
      lcdui.init();
    }
  }

  void SDMenuPopCallback() {
    if (card.isDetected()) lcdui.goto_screen(menu_sdcard);
  }

#endif

void StopPopCallback() {
  #if HAS_LCD_MENU
    lcdui.goto_screen(menu_stop_print);
  #else
    lcdui.stop_print();
  #endif
}

void PlayPausePopCallback() {
  if (printer.isPrinting())     lcdui.pause_print();
  else if (printer.isPaused())  lcdui.resume_print();
}

#if ENABLED(RFID_MODULE)

  void rfidPopCallback(NexObject *nexobject) {
    ZERO(buffer);

    String temp = "M522 ";
    uint16_t Rfid_read = nexlcd.getValue(RfidR);

    if (nexobject == &Rfid0)
      temp += "T0 ";
    else if (nexobject == &Rfid1)
      temp += "T1 ";
    else if (nexobject == &Rfid2)
      temp += "T2 ";
    else if (nexobject == &Rfid3)
      temp += "T3 ";
    else if (nexobject == &Rfid4)
      temp += "T4 ";
    else if (nexobject == &Rfid5)
      temp += "T5 ";

    if(Rfid_read)
      temp += "R";
    else
      temp += "W";

    temp.toCharArray(buffer, NEXTION_BUFFER_SIZE);
    commands.enqueue_one_now(buffer);
  }

  void rfid_setText(PGM_P message, uint32_t color/*=65535*/) {
    char Rfid_status_message[25];
    strncpy(Rfid_status_message, message, 30);
    nexlcd.Set_font_color_pco(RfidText, color);
    nexlcd.setText(RfidText, Rfid_status_message);
  }

#endif

void sethotPopCallback() {

  uint16_t  Heater      = nexlcd.getValue(Theater),
            temperature = nexlcd.getValue(Tset);

  #if BEDS > 0
    if (Heater == 2)
      beds[0].setTarget(temperature);
    else
  #endif
  #if CHAMBERS > 0
    if (Heater == 3)
      chambers[0].setTarget(temperature);
    else
  #endif
  #if HAS_TEMP_HOTEND
    hotends[(uint8_t)Heater].setTarget(temperature);
  #endif

  nexlcd.show(Pprinter);
}

void setgcodePopCallback() {
  ZERO(buffer);
  nexlcd.getText(Tgcode, buffer);
  nexlcd.setText(Tgcode, "");
  commands.enqueue_one_now(buffer);
}

#if FAN_COUNT > 0
  void setfanPopCallback() {
    fans[0].speed = (fans[0].speed ? 0 : 255);
    nexlcd.setValue(Fanspeed, fans[0].percent());
  }
#endif

#if HAS_CASE_LIGHT
  void setlightPopCallback() {
    caselight.status = !caselight.status;
    caselight.update();
  }
#endif

void setmovePopCallback() {

  #if EXTRUDERS > 1
    const uint8_t temp_extruder = tools.active_extruder;
    char temp[5] = { 0 };

    ZERO(buffer);
    itoa(nexlcd.getValue(ext), temp, 2);
    strcat(buffer, "T");
    strcat(buffer, temp);
    commands.enqueue_one_now(buffer);
  #endif

  ZERO(buffer);
  nexlcd.getText(movecmd, buffer);
  commands.enqueue_now_P(PSTR("G91"));
  commands.enqueue_one_now(buffer);
  commands.enqueue_now_P(PSTR("G90"));

  #if EXTRUDERS > 1
    ZERO(buffer);
    itoa(temp_extruder, temp, 2);
    strcat(buffer, "T");
    strcat(buffer, temp);
    commands.enqueue_one_now(buffer);
  #endif
}

void motoroffPopCallback() {
  commands.enqueue_now_P(PSTR("M84"));
}

void filamentPopCallback(NexObject *nexobject) {
  ZERO(buffer);
  nexlcd.getText(Filgcode, buffer);
  if (nexobject == &FilExtr)
    commands.enqueue_one_now(buffer);
  else {
    commands.enqueue_now_P(PSTR("G91"));
    commands.enqueue_one_now(buffer);
    commands.enqueue_now_P(PSTR("G90"));
  }
}

static void degtoLCD(const uint8_t h, uint16_t temp) {
  NOMORE(temp, 999);
  nexlcd.setValue(*heater_list0[h], temp);
}

static void targetdegtoLCD(const uint8_t h, const uint16_t temp) {
  nexlcd.setValue(*heater_list1[h], temp);
}

static void coordtoLCD() {
  char* valuetemp;
  ZERO(buffer);

  if (PageID == 2) {
    nexlcd.setText(LcdX, ftostr41sign(LOGICAL_X_POSITION(mechanics.current_position[X_AXIS])));
    nexlcd.setText(LcdY, ftostr41sign(LOGICAL_Y_POSITION(mechanics.current_position[Y_AXIS])));
    nexlcd.setText(LcdZ, ftostr41sign(FIXFLOAT(LOGICAL_Z_POSITION(mechanics.current_position[Z_AXIS]))));
  }
  else if (PageID == 4) {
    if (mechanics.home_flag.XHomed) {
      valuetemp = ftostr4sign(LOGICAL_X_POSITION(mechanics.current_position[X_AXIS]));
      strcat(buffer, "X");
      strcat(buffer, valuetemp);
    }
    else
      strcat(buffer, "?");

    if (mechanics.home_flag.YHomed) {
      valuetemp = ftostr4sign(LOGICAL_Y_POSITION(mechanics.current_position[Y_AXIS]));
      strcat(buffer, " Y");
      strcat(buffer, valuetemp);
    }
    else
      strcat(buffer, " ?");

    if (mechanics.home_flag.ZHomed) {
      valuetemp = ftostr52sp(FIXFLOAT(LOGICAL_Z_POSITION(mechanics.current_position[Z_AXIS])));
      strcat(buffer, " Z");
      strcat(buffer, valuetemp);
    }
    else
      strcat(buffer, " ?");

    nexlcd.setText(LedCoord5, buffer);
  }
}

void Nextion_draw_update() {

  static uint8_t  PreviousPage                = 0,
                  Previousfeedrate            = 0,
                  PreviousfanSpeed            = 0,
                  PreviouspercentDone         = 0;
  static uint16_t PrevioustargetdegHeater[3]  = { 0.0 };

  #if ENABLED(NEXTION_GFX)           
    static bool GfxVis = false;
  #endif

  if (!NextionON || PageID == 11) return;

  const uint8_t temp_PageID = nexlcd.pageID();
  if (temp_PageID != NULL) PageID = temp_PageID;

  switch (PageID) {

    case 2:

      #if ENABLED(NEXTION_GFX)
        if (printer.isPrinting()) {
          if (!GfxVis) {
            GfxVis = true;
            nexlcd.sendCommandPGM(PSTR("prt.val=1"));
          }
        }
        else {
          if (GfxVis) {
            GfxVis = false;
            nexlcd.sendCommandPGM(PSTR("prt.val=0"));
          }
        }
      #endif

      if (PreviousPage != 2) {
        lcdui.set_status(lcdui.status_message);
        #if ENABLED(NEXTION_GFX)
          #if MECH(DELTA)
            gfx_clear(mechanics.data.print_radius * 2, mechanics.data.print_radius * 2, mechanics.data.height);
          #else
            gfx_clear(X_MAX_BED, Y_MAX_BED, Z_MAX_BED);
          #endif
        #endif
      }

      #if FAN_COUNT > 0
        if (PreviousfanSpeed != fans[0].actual_speed()) {
          nexlcd.setValue(Fanspeed, fans[0].percent());
          PreviousfanSpeed = fans[0].actual_speed();
        }
      #endif

      #if HAS_CASE_LIGHT
        nexlcd.setValue(LightStatus, caselight.status ? 2 : 1);
      #endif

      if (Previousfeedrate != mechanics.feedrate_percentage) {
        nexlcd.setValue(VSpeed, mechanics.feedrate_percentage);
        Previousfeedrate = mechanics.feedrate_percentage;
      }

      #if HAS_TEMP_HE0
        degtoLCD(0, hotends[0].current_temperature);
        if (PrevioustargetdegHeater[0] != uint16_t(hotends[0].target_temperature)) {
          PrevioustargetdegHeater[0] = uint16_t(hotends[0].target_temperature);
          targetdegtoLCD(0, PrevioustargetdegHeater[0]);
        }
      #endif
      #if HAS_TEMP_HE1
        degtoLCD(1, hotends[1].current_temperature);
        if (PrevioustargetdegHeater[1] != uint16_t(hotends[1].target_temperature)) {
          PrevioustargetdegHeater[1] = uint16_t(hotends[1].target_temperature);
          targetdegtoLCD(1, PrevioustargetdegHeater[1]);
        }
      #elif CHAMBERS > 0
        degtoLCD(3, chambers[0].current_temperature);
        if (PrevioustargetdegHeater[1] != uint16_t(chambers[0].target_temperature)) {
          PrevioustargetdegHeater[1] = uint16_t(chambers[0].target_temperature);
          targetdegtoLCD(3, PrevioustargetdegHeater[1]);
        }
      #elif ENABLED(DHT_SENSOR)
        if (lcdui.get_blink(3))
          degtoLCD(4, dhtsensor.Humidity + 500);
        else
          degtoLCD(4, dhtsensor.Temperature);
      #endif
      #if BEDS > 0
        degtoLCD(2, beds[0].current_temperature);
        if (PrevioustargetdegHeater[2] != uint16_t(beds[0].target_temperature)) {
          PrevioustargetdegHeater[2] = uint16_t(beds[0].target_temperature);
          targetdegtoLCD(2, PrevioustargetdegHeater[2]);
        }
      #endif

      coordtoLCD();

      if (PreviouspercentDone != printer.progress) {
        // Progress bar solid part
        nexlcd.setValue(progressbar, printer.progress);
        // Estimate End Time
        ZERO(buffer);
        char buffer1[10];
        uint8_t digit;
        duration_t Time = print_job_counter.duration();
        digit = Time.toDigital(buffer1, true);
        strcat(buffer, "S");
        strcat(buffer, buffer1);
        Time = (print_job_counter.duration() * (100 - printer.progress)) / (printer.progress + 0.1);
        digit += Time.toDigital(buffer1, true);
        if (digit > 14)
          strcat(buffer, "E");
        else
          strcat(buffer, " E");
        strcat(buffer, buffer1);
        nexlcd.setText(LcdTime, buffer);
        PreviouspercentDone = printer.progress;
      }

      if (printer.isPrinting()) {
        if (SDstatus != SD_HOST_PRINTING) {
          SDstatus = SD_HOST_PRINTING;
          nexlcd.setValue(SD, SDstatus);
        }
      }
      else if (printer.isPaused()) {
        if (SDstatus != SD_HOST_PAUSE) {
          SDstatus = SD_HOST_PAUSE;
          nexlcd.setValue(SD, SDstatus);
        }
      }
      else if (IS_SD_OK()) {
        if (SDstatus != SD_INSERT) {
          SDstatus = SD_INSERT;
          nexlcd.setValue(SD, SDstatus);
        }
      }
      #if HAS_SD_SUPPORT
        else if (!IS_SD_OK()) {
          if (SDstatus != SD_NO_INSERT) {
            SDstatus = SD_NO_INSERT;
            nexlcd.setValue(SD, SDstatus);
          }
        }
      #else
        else {
          if (SDstatus != NO_SD) {
            SDstatus = NO_SD;
            nexlcd.setValue(SD, SDstatus);
          }
        }
      #endif

      break;

    case 4:
      coordtoLCD();
      break;

    case 5: {
      const uint16_t temp_speed = nexlcd.getValue(VSpeed);
      if (temp_speed != NULL)
        Previousfeedrate = mechanics.feedrate_percentage = temp_speed;
      break;
    }

    default: break;

  }

  PreviousPage = PageID;

}

#if ENABLED(NEXTION_GFX)
  void gfx_origin(const float x, const float y, const float z) {
    gfx.origin(x, y, z);
  }

  void gfx_scale(const float scale) {
    gfx.set_scale(scale);
  }

  void gfx_clear(const float x, const float y, const float z) {
    if (PageID == 2 && printer.isPrinting())
      gfx.clear(x, y, z);
  }

  void gfx_cursor_to(const float x, const float y, const float z, bool force_cursor) {
    if (PageID == 2 && (printer.isPrinting() || force_cursor))
      gfx.cursor_to(x, y, z);
  }

  void gfx_line_to(const float x, const float y, const float z) {
    if (PageID == 2 && printer.isPrinting()) {
      #if ENABLED(ARDUINO_ARCH_SAM)
        gfx.line_to(NX_TOOL, x, y, z, true);
      #else
        gfx.line_to(NX_TOOL, x, y, z);
      #endif
    }
  }

  void gfx_plane_to(const float x, const float y, const float z) {
    uint8_t color;
    if (PageID == 2) {
      if (z < 10) color = NX_LOW;
      else color = NX_HIGH;
      gfx.line_to(color, x, y, z, true);
    }
  }
#endif

#if HAS_LCD_MENU

  constexpr uint16_t  hot_color = 63488,
                      sel_color = 2016,
                      txt_color = 65535;

  // Page txtmenu touch listen
  NexObject *txtmenu_list[] =
  {
    &EncRow1,
    &EncRow2,
    &EncRow3,
    &EncRow4,
    &EncRow5,
    &EncRow6,
    &EncUp,
    &EncDown,
    &EncSend,
    &EncExit,
    NULL
  };

  bool LcdUI::use_click() {
    const bool click = lcd_clicked;
    lcd_clicked = false;
    return click;
  }

  void encoderPopCallback(NexObject *nexobject) {
    // Click on encoder
    if (nexobject == &EncExit)
      lcdui.return_to_status();
    else if (nexobject == &EncUp)
      lcdui.encoderPosition += lcdui.encoderDirection;
    else if (nexobject == &EncDown)
      lcdui.encoderPosition -= lcdui.encoderDirection;
    else if (nexobject == &EncSend) {
      lcdui.lcd_clicked = true;
      printer.setWaitForUser(false);
    }

    // Click on text row
    if (line_encoder_touch) {
      for (uint8_t row = 0; row < 6; row++) {
        if (nexobject == txtmenu_list[row]) {
          lcdui.encoderPosition = row + encoderTopLine;
          lcdui.lcd_clicked = true;
        }
      }
    }

    #if LCD_TIMEOUT_TO_STATUS
      return_to_status_ms = millis();
    #endif

    lcdui.refresh(LCDVIEW_REDRAW_NOW);

  }

  inline static void nextion_put_space(const uint8_t max_length) {
    for (uint8_t i = 0; i < max_length; i++)
      nexlcd.setChar(' ');
  }

  inline static void nextion_put_str_P(PGM_P str) {
    const uint8_t len = strlen_P(str);
    for (uint8_t i = 0; i < len; i++) {
      char ch = pgm_read_byte(str++);
      nexlcd.setChar(ch);
    }
  }

  inline static void nextion_put_str(const char * str) {
    const uint8_t len = strlen(str);
    for (uint8_t i = 0; i < len; i++)
      nexlcd.setChar(*str++);
  }

  inline static void mark_as_selected(const uint8_t row, const bool sel) {
    const uint16_t color = sel ? sel_color : txt_color;
    nexlcd.Set_font_color_pco(*txtmenu_list[row], color);
  }

  void _wrap_string(uint8_t &y, const char * const string, read_byte_cb_t cb_read_byte, bool wordwrap/*=false*/) {
    if (!string) return;
    uint8_t x = 0;
    nexlcd.startChar(*txtmenu_list[y]);
    uint8_t *p = (uint8_t*)string;
    if (wordwrap) {
      uint8_t *wrd = p, c = 0;
      for (;;) {
        wchar_t ch;
        p = get_utf8_value_cb(p, cb_read_byte, &ch);
        const bool eol = !ch;
        if (eol || ch == ' ' || ch == '-' || ch == '+' || ch == '.') {
          if (!c && ch == ' ') continue;  // collapse extra spaces
          if (x + c > LCD_WIDTH && c < (LCD_WIDTH) * 3 / 4) { // should it wrap?
            x = 0; y++;                   // move x to string len (plus space)
            nexlcd.endChar();             // simulate carriage return
            nexlcd.startChar(*txtmenu_list[y]);
          }
          c += !eol;                      // +1 so the space will be printed
          x += c;                         // advance x to new position
          while (c--) {                   // character countdown
            wrd = get_utf8_value_cb(wrd, cb_read_byte, &ch); // get characters again
            nexlcd.setChar(ch);           // word (plus space) to the LCD
          }
          nexlcd.setChar(' ');
          if (eol) {                      // all done
            nexlcd.endChar();
            break;
          }
          wrd = p;                        // set up for next word
        }
        else
          c++;                            // count word characters
      }
    }
    else {
      for (;;) {
        wchar_t ch;
        p = get_utf8_value_cb(p, cb_read_byte, &ch);
        if (!ch) {
          nexlcd.endChar();
          break;
        }
        nexlcd.setChar(ch);
        x++;
        if (x >= LCD_WIDTH) {
          x = 0; y++;
          nexlcd.endChar();
          nexlcd.startChar(*txtmenu_list[y]);
        }
      }
    }
  }

  // Draw a static line of text in the same idiom as a menu item
  void draw_menu_item_static(const uint8_t row, PGM_P const pstr, const bool center/*=true*/, const bool invert/*=false*/, const char* valstr/*=NULL*/) {
    UNUSED(center);
    line_encoder_touch = true;
    mark_as_selected(row, invert);
    nexlcd.startChar(*txtmenu_list[row]);
    nextion_put_str_P(pstr);
    if (valstr != NULL) nextion_put_str(valstr);
    nexlcd.endChar();
  }

  // Draw a generic menu item
  void draw_menu_item(const bool sel, const uint8_t row, PGM_P const pstr, const char pre_char, const char post_char) {
    UNUSED(pre_char); UNUSED(post_char);
    line_encoder_touch = true;
    mark_as_selected(row, sel);
    nexlcd.startChar(*txtmenu_list[row]);
    nextion_put_str_P(pstr);
    nexlcd.endChar();
  }

  // Draw a menu item with an editable value
  void _draw_menu_item_edit(const bool sel, const uint8_t row, PGM_P const pstr, const char* const data, const bool pgm) {
    const uint8_t labellen  = strlen_P(pstr),
                  vallen = (pgm ? strlen_P(data) : strlen((char*)data));
    line_encoder_touch = true;
    mark_as_selected(row, sel);
    nexlcd.startChar(*txtmenu_list[row]);
    nextion_put_str_P(pstr);
    nextion_put_str_P(PSTR(":"));
    nextion_put_space(LCD_WIDTH - labellen - vallen - 1);
    if (pgm)
      nextion_put_str_P(data);
    else
      nextion_put_str((char*)data);
    nexlcd.endChar();
  }

  void draw_edit_screen(PGM_P const pstr, const char* const value/*=nullptr*/) {

    const uint8_t labellen  = strlen_P(pstr),
                  vallen    = strlen(value);

    bool extra_row = labellen > LCD_WIDTH - vallen - 1;

    constexpr uint8_t row = 2;

    line_encoder_touch = false;

    nexlcd.Set_font_color_pco(*txtmenu_list[row], sel_color);

    if (extra_row) {
      nexlcd.Set_font_color_pco(*txtmenu_list[row - 1], sel_color);
      nexlcd.startChar(*txtmenu_list[row - 1]);
      nextion_put_str_P(pstr);
      nexlcd.endChar();
      nexlcd.startChar(*txtmenu_list[row]);
      nextion_put_space(LCD_WIDTH - vallen);
      nextion_put_str(value);
    }
    else {
      nexlcd.startChar(*txtmenu_list[row]);
      nextion_put_str_P(pstr);
      nextion_put_str_P(PSTR(":"));
      nextion_put_space(LCD_WIDTH - labellen - vallen - 1);
      nextion_put_str(value);
    }
    nexlcd.endChar();
  }

  inline void draw_select_screen_prompt(PGM_P const pref, const char * const string/*=nullptr*/, PGM_P const suff/*=nullptr*/) {
    const uint8_t plen = strlen_P(pref), slen = suff ? strlen_P(suff) : 0;
    uint8_t y = 1;
    wrap_string_P(y, pref, true);
    if (string) {
      y++;
      wrap_string(y, string);
    }
    if (suff) wrap_string_P(y, suff);
  }

  void draw_select_screen(PGM_P const yes, PGM_P const no, const bool yesno, PGM_P const pref, const char * const string, PGM_P const suff) {
    draw_select_screen_prompt(pref, string, suff);
    nexlcd.startChar(*txtmenu_list[LCD_HEIGHT - 1]);
    nextion_put_str_P(yesno ? PSTR(" ") : PSTR("["));
    nextion_put_str_P(no);
    nextion_put_str_P(yesno ? PSTR(" ") : PSTR("]"));
    nextion_put_space(2);
    nextion_put_str_P(yesno ? PSTR("[") : PSTR(" "));
    nextion_put_str_P(yes);
    nextion_put_str_P(yesno ? PSTR("]") : PSTR(" "));
    nexlcd.endChar();
  }

  #if HAS_SD_SUPPORT

    void draw_sd_menu_item(const bool sel, const uint8_t row, PGM_P const pstr, SDCard &theCard, const bool isDir) {
      UNUSED(pstr);
      mark_as_selected(row, sel);
      nexlcd.startChar(*txtmenu_list[row]);
      if (isDir) nextion_put_str_P(PSTR(LCD_STR_FOLDER));
      nextion_put_str(theCard.fileName);
      nexlcd.endChar();
    }

  #endif // SDSUPPORT

  /**
   * If the most recent manual move hasn't been fed to the planner yet,
   * and the planner can accept one, send a move immediately.
   */
  void LcdUI::manage_manual_move() {

    if (processing_manual_move) return;

    if (manual_move_axis != (int8_t)NO_AXIS && expired(&manual_move_ms, (move_menu_scale < 0.99f ? 1U : 250U)) && !planner.is_full()) {

      #if IS_KINEMATIC

        const float old_feedrate = mechanics.feedrate_mm_s;
        mechanics.feedrate_mm_s = MMM_TO_MMS(manual_feedrate_mm_m[manual_move_axis]);

        #if EXTRUDERS > 1
          const int8_t old_extruder = tools.active_extruder;
          if (manual_move_axis == E_AXIS) tools.active_extruder = manual_move_e_index;
        #endif

        // Set movement on a single axis
        mechanics.set_destination_to_current();
        mechanics.destination[manual_move_axis] += manual_move_offset;

        // Reset for the next move
        manual_move_offset = 0;
        manual_move_axis = (int8_t)NO_AXIS;

        // DELTA and SCARA machines use segmented moves, which could fill the planner during the call to
        // move_to_destination. This will cause idle() to be called, which can then call this function while the
        // previous invocation is being blocked. Modifications to manual_move_offset shouldn't be made while
        // processing_manual_move is true or the planner will get out of sync.
        processing_manual_move = true;
        mechanics.prepare_move_to_destination(); // will call set_current_to_destination
        processing_manual_move = false;

        mechanics.feedrate_mm_s = old_feedrate;
        #if EXTRUDERS > 1
          tools.active_extruder = old_extruder;
        #endif

      #else

        planner.buffer_line(mechanics.current_position, MMM_TO_MMS(manual_feedrate_mm_m[manual_move_axis]), tools.active_extruder);
        manual_move_axis = (int8_t)NO_AXIS;

      #endif
    }
  }

#endif

static inline void PopCallback(NexObject *nexobject) {

  if (nexobject == &Tenter)             sethotPopCallback();
  else if ( nexobject == &XYHome    ||
            nexobject == &XYUp      ||
            nexobject == &XYRight   ||
            nexobject == &XYDown    ||
            nexobject == &XYLeft    ||
            nexobject == &ZHome     ||
            nexobject == &ZUp       ||
            nexobject == &ZDown     ||
            nexobject == &Extrude   ||
            nexobject == &Retract)      setmovePopCallback();
  else if ( nexobject == &MotorOff)     motoroffPopCallback();
  else if ( nexobject == &Send)         setgcodePopCallback();

  else if ( nexobject == &FilLoad   ||
            nexobject == &FilUnload ||
            nexobject == &FilExtr)      filamentPopCallback(nexobject);

  #if HAS_SD_SUPPORT
    else if (nexobject == &NextionFW)   lcdui.goto_screen(menu_firmware);
  #endif

  #if HAS_LCD_MENU
    else if (nexobject == &TxtMenu)     lcdui.goto_screen(menu_main);
  #endif

  #if FAN_COUNT > 0
    else if (nexobject == &FanTouch)    setfanPopCallback();
  #endif
  #if HAS_CASE_LIGHT
    else if (nexobject == &Light)       setlightPopCallback();
  #endif
  #if ENABLED(RFID_MODULE)
    else if ( nexobject == &Rfid0   ||
              nexobject == &Rfid1   ||
              nexobject == &Rfid2   ||
              nexobject == &Rfid3   ||
              nexobject == &Rfid4   ||
              nexobject == &Rfid5)      rfidPopCallback(nexobject);
  #endif
  #if HAS_LCD_MENU
    else if ( nexobject == &EncRow1 ||
              nexobject == &EncRow2 ||
              nexobject == &EncRow3 ||
              nexobject == &EncRow4 ||
              nexobject == &EncRow5 ||
              nexobject == &EncRow6 ||
              nexobject == &EncUp   ||
              nexobject == &EncDown ||
              nexobject == &EncSend ||
              nexobject == &EncExit)    encoderPopCallback(nexobject);
  #endif
  #if HAS_SD_SUPPORT
    else if (nexobject == &SDMenu)      SDMenuPopCallback();
  #endif
  else if (nexobject == &NStop)       StopPopCallback();
  else if (nexobject == &NPlay)       PlayPausePopCallback();

}

// Check the push button
static void Nextion_parse_key_touch(NexObject *list[]) {
  bool str_start_flag = false;
  uint8_t cnt_0xFF  = 0,
          index     = 0;

  ZERO(buffer);
  while (nexSerial.available()) {
    uint8_t c = nexSerial.read();
    if (c == NEX_RET_EVENT_TOUCH_HEAD) {
      str_start_flag = true;
      HAL::delayMilliseconds(10);
    }
    else if (str_start_flag) {
      if (c == 0xFF) cnt_0xFF++;                    
      buffer[index++] = (char)c;
      if (cnt_0xFF >= 3 || index == sizeof(buffer)) break;
    }
  }

  if (cnt_0xFF >= 3) {
    const uint8_t pid = buffer[0];
    const uint8_t cid = buffer[1];
    const int32_t event = (int32_t)buffer[2];
    for (uint8_t i = 0; list[i] != NULL; i++) {
      if (list[i]->pid == pid && list[i]->cid == cid) {
        if (event == NEX_EVENT_POP) PopCallback(list[i]);
        break;
      }
    }
  }
}

static void Nextion_update_buttons() {

  #if HAS_LCD_MENU

    if (PageID == 11) {
      // Read button Encoder touch
      Nextion_parse_key_touch(txtmenu_list);
    }
    else {
      // Read all button into Nextion LCD
      Nextion_parse_key_touch(nex_listen_list);
    }

  #else // !HAS_LCD_MENU

    // Read all button into Nextion LCD
    Nextion_parse_key_touch(nex_listen_list);

  #endif

}

/**
 * LcdUI Function
 */
void LcdUI::clear_lcd() {
  PageID == 11;
  nexlcd.show(Ptxtmenu);
}

void LcdUI::init() {

  for (uint8_t i = 0; i < 10; i++) {
    ZERO(buffer);
    NextionON = nexlcd.init(buffer);
    if (NextionON) break;
    HAL::delayMilliseconds(1000);
  }

  if (!NextionON) {
    SERIAL_LM(ER, "Nextion not connected!");
    return;
  }
  else {
    SERIAL_MSG("Nextion");
    // Get Model

    if (strstr_P(buffer, PSTR("3224"))) {       // Model 2.4" or 2.8" Normal or Enhanced
      SERIAL_MSG(" 2.4");
      #if ENABLED(NEXTION_GFX)
        gfx.set_position(1, 24, 250, 155);
      #endif
    }
    else if (strstr_P(buffer, PSTR("4024"))) {  // Model 3.2" Normal or Enhanced
      SERIAL_MSG(" 3.2");
      #if ENABLED(NEXTION_GFX)
        gfx.set_position(1, 24, 250, 155);
      #endif
    }
    else if (strstr_P(buffer, PSTR("4832"))) {  // Model 3.5" Normal or Enhanced
      SERIAL_MSG(" 3.5");
      #if ENABLED(NEXTION_GFX)
        gfx.set_position(1, 24, 250, 155);
      #endif
    }
    else if (strstr_P(buffer, PSTR("4827"))) {  // Model 4.3" Normal or Enhanced
      SERIAL_MSG(" 4.3");
      #if ENABLED(NEXTION_GFX)
        gfx.set_position(1, 24, 250, 155);
      #endif
    }
    else if (strstr_P(buffer, PSTR("8048"))) {  // Model 7" Normal or Enhanced
      SERIAL_MSG(" 7");
      #if ENABLED(NEXTION_GFX)
        gfx.set_position(274, 213, 250, 155);
      #endif
    }
    SERIAL_CHR('"'); SERIAL_EM(" connected!");

    #if ENABLED(NEXTION_GFX)
      gfx.color_set(NX_AXIS + X_AXIS, 63488);
      gfx.color_set(NX_AXIS + Y_AXIS, 2016);
      gfx.color_set(NX_AXIS + Z_AXIS, 31);
      gfx.color_set(NX_MOVE, 2047);
      gfx.color_set(NX_TOOL, 65535);
      gfx.color_set(NX_LOW, 2047);
      gfx.color_set(NX_HIGH, 63488);
    #endif

    nexlcd.sendCommandPGM(PSTR("get pg0.va1.val"));
    const uint16_t nextion_version = nexlcd.recvRetNumber();

    // Start timer for logo anim
    nexlcd.sendCommandPGM(PSTR("tm0.en=1"));

    setpagePrinter();

    #if LCD_TIMEOUT_TO_STATUS
      return_to_status_ms = millis();
    #endif

    #if HAS_LCD_MENU
      // Check the Nextion Firmware
      if (nextion_version < NEXTION_LCD_FIRMWARE_VERSION) lcdui.goto_screen(menu_nextion);
    #endif

  }
}

bool LcdUI::get_blink(uint8_t moltiplicator/*=1*/) {
  static uint8_t blink = 0;
  static millis_s next_blink_ms = 0;
  if (expired(&next_blink_ms, millis_s(1000U * moltiplicator))) blink ^= 0xFF;
  return blink != 0;
}

void LcdUI::kill_screen(PGM_P lcd_msg) {
  set_alert_status_P(lcd_msg);
}

void LcdUI::update() {
  static millis_s next_lcd_update_ms = 0;

  if (!NextionON) return;

  update_buttons();

  #if HAS_LCD_MENU
    // Handle any queued Move Axis motion
    manage_manual_move();
  #endif

  #if HAS_SD_SUPPORT && PIN_EXISTS(SD_DETECT)

    const uint8_t sd_status = (uint8_t)IS_SD_INSERTED();
    if (sd_status != lcd_sd_status && detected()) {

      if (sd_status) {
        HAL::delayMilliseconds(500);  // Some boards need a delay to get settled
        card.mount();
        if (lcd_sd_status == 2)
          card.beginautostart();  // Initial boot
        else
          set_status_P(PSTR(MSG_SD_INSERTED));
      }
      else {
        card.unmount();
        if (lcd_sd_status != 2) set_status_P(PSTR(MSG_SD_REMOVED));
      }

      lcd_sd_status = sd_status;

      refresh();
    }

  #endif // HAS_SD_SUPPORT && SD_DETECT_PIN

  if (expired(&next_lcd_update_ms, LCD_UPDATE_INTERVAL))
    Nextion_draw_update();

  #if HAS_LCD_MENU

    if (PageID == 11) {

      #if LCD_TIMEOUT_TO_STATUS
        if (defer_return_to_status)
          return_to_status_ms = millis();
        else if (expired(&return_to_status_ms, millis_l(LCD_TIMEOUT_TO_STATUS)))
          return_to_status();
      #endif

      switch (lcdDrawUpdate) {
        case LCDVIEW_CALL_NO_REDRAW:
          refresh(LCDVIEW_NONE);
          break;
        case LCDVIEW_CLEAR_CALL_REDRAW:
        case LCDVIEW_CALL_REDRAW_NEXT:
          refresh(LCDVIEW_REDRAW_NOW);
        case LCDVIEW_REDRAW_NOW:
        case LCDVIEW_NONE:
          break;
      } // switch

      lcdui.run_current_screen();

      switch (lcdDrawUpdate) {
        case LCDVIEW_CLEAR_CALL_REDRAW:
          clear_lcd(); break;
        case LCDVIEW_REDRAW_NOW:
          refresh(LCDVIEW_NONE);
        case LCDVIEW_NONE:
        case LCDVIEW_CALL_REDRAW_NEXT:
        case LCDVIEW_CALL_NO_REDRAW:
        default: break;
      } // switch

    }
    else {
      #if LCD_TIMEOUT_TO_STATUS
        return_to_status_ms = millis();
      #endif
    }

  #endif

}

bool LcdUI::detected() { return NextionON; }

void LcdUI::quick_feedback(const bool clear_buttons/*=true*/) {
  UNUSED(clear_buttons);
  #if HAS_LCD_MENU
    refresh();
  #endif
  // Buzz and wait. The delay is needed for buttons to settle!
  sound.playtone(LCD_FEEDBACK_FREQUENCY_DURATION_MS, LCD_FEEDBACK_FREQUENCY_HZ);
}

void LcdUI::set_alert_status_P(PGM_P const message) {
  set_status_P(message, 1);
  #if HAS_LCD_MENU
    return_to_status();
  #endif
}

bool LcdUI::has_status() { return (status_message[0] != '\0'); }

void LcdUI::set_status(const char* const message, bool persist) {
  UNUSED(persist);
  if (status_message_level > 0 || !NextionON) return;
  strncpy(status_message, message, NEXTION_MAX_MESSAGE_LENGTH);
  if (PageID == 2) nexlcd.setText(LcdStatus, status_message);
}

void LcdUI::set_status_P(PGM_P const message, int8_t level/*=0*/) {
  if (level < 0) level = status_message_level = 0;
  if (level < status_message_level || !NextionON) return;
  status_message_level = level;

  // Get a pointer to the null terminator
  PGM_P pend = message + strlen_P(message);

  while ((pend - message) > NEXTION_MAX_MESSAGE_LENGTH) {
    --pend;
    while (!((pgm_read_byte(pend) & 0xC0u) != 0x80u)) --pend;
  };

  uint8_t maxLen = pend - message;
  strncpy_P(status_message, message, maxLen);
  status_message[maxLen] = '\0';

  if (PageID == 2) nexlcd.setText(LcdStatus, status_message);
}

void LcdUI::status_printf_P(const uint8_t level, PGM_P const fmt, ...) {
  if (level < status_message_level || !NextionON) return;
  status_message_level = level;
  va_list args;
  va_start(args, fmt);
  vsnprintf(status_message, NEXTION_MAX_MESSAGE_LENGTH, fmt, args);
  va_end(args);
  if (PageID == 2) nexlcd.setText(LcdStatus, status_message);
}

void LcdUI::reset_status() {
  static const char paused[] PROGMEM = MSG_PRINT_PAUSED;
  static const char printing[] PROGMEM = MSG_PRINTING;
  static const char welcome[] PROGMEM = WELCOME_MSG;
  #if ENABLED(SERVICE_TIME_1)
    static const char service1[] PROGMEM = { "> " SERVICE_NAME_1 "!" };
  #endif
  #if ENABLED(SERVICE_TIME_2)
    static const char service2[] PROGMEM = { "> " SERVICE_NAME_2 "!" };
  #endif
  #if ENABLED(SERVICE_TIME_3)
    static const char service3[] PROGMEM = { "> " SERVICE_NAME_3 "!" };
  #endif
  PGM_P msg;
  if (print_job_counter.isPaused())
    msg = paused;
  #if HAS_SD_SUPPORT
    else if (IS_SD_PRINTING())
      return lcdui.set_status(card.fileName, true);
  #endif
  else if (print_job_counter.isRunning())
    msg = printing;
  #if ENABLED(SERVICE_TIME_1)
    else if (print_job_counter.needService(1)) msg = service1;
  #endif
  #if ENABLED(SERVICE_TIME_2)
    else if (print_job_counter.needService(2)) msg = service2;
  #endif
  #if ENABLED(SERVICE_TIME_3)
    else if (print_job_counter.needService(3)) msg = service3;
  #endif
  else
    msg = welcome;

  lcdui.set_status_P(msg, -1);
}

void LcdUI::status_screen() {
  if (PageID == 11) {
    PageID = 2;
    nexlcd.show(Pprinter);
  }
}

bool LcdUI::button_pressed() { return lcd_clicked; }

void LcdUI::update_buttons() {
  Nextion_update_buttons();
  #if ENABLED(AUTO_BED_LEVELING_UBL)
    if (lcdui.external_control)
      ubl.encoder_diff = lcdui.encoderPosition;
  #endif
}

/**
 * Print pause, resume and stop
 */
void LcdUI::pause_print() {
  #if HAS_SD_RESTART
    if (restart.enabled && IS_SD_PRINTING()) restart.save_job(true, false);
  #endif

  host_action.prompt_open(PROMPT_PAUSE_RESUME, PSTR("LCD Pause"), PSTR("Resume"));

  #if ENABLED(PARK_HEAD_ON_PAUSE)
    lcd_pause_show_message(PAUSE_MESSAGE_PAUSING, PAUSE_MODE_PAUSE_PRINT);  // Show message immediately to let user know about pause in progress
    commands.enqueue_now_P(PSTR("M25 P\nM24"));
  #elif HAS_SD_SUPPORT
    commands.enqueue_now_P(PSTR("M25"));
  #else
    host_action.pause();
  #endif

  planner.synchronize();

}

void LcdUI::resume_print() {
  #if HAS_SD_SUPPORT
    commands.enqueue_now_P(PSTR("M24"));
  #else
    host_action.resume();
  #endif
}

void LcdUI::stop_print() {
  #if HAS_SD_SUPPORT
    printer.setWaitForHeatUp(false);
    printer.setWaitForUser(false);
    if (IS_SD_PRINTING()) card.setAbortSDprinting(true);
  #endif
  host_action.prompt_open(PROMPT_INFO, PSTR("Lcd Abort"));
  host_action.cancel();
  set_status_P(PSTR(MSG_PRINT_ABORTED), -1);
  return_to_status();
}

#if ENABLED(AUTO_BED_LEVELING_UBL)
  void LcdUI::ubl_plot(const uint8_t x_plot, const uint8_t y_plot) {}
#endif

#if ENABLED(AUTO_BED_LEVELING_UBL) || ENABLED(G26_MESH_VALIDATION)
  void LcdUI::wait_for_release() { HAL::delayMilliseconds(50); }
#endif

#if ENABLED(ADVANCED_PAUSE_FEATURE)

  void LcdUI::draw_hotend_status(const uint8_t row, const uint8_t hotend) {

    UNUSED(row);

    static millis_s nex_update_ms = 0;

    if (expired(&nex_update_ms, 1500U)) {

      ZERO(buffer);
      strcat(buffer, MSG_FILAMENT_CHANGE_NOZZLE "H");
      strcat(buffer, ui8tostr1(hotend));
      strcat(buffer, " ");
      strcat(buffer, i16tostr3(hotends[hotend].current_temperature));
      strcat(buffer, "/");

      if (get_blink() || !hotends[hotend].isIdle())
        strcat(buffer, i16tostr3(hotends[hotend].target_temperature));

      nexlcd.Set_font_color_pco(*txtmenu_list[LCD_HEIGHT - 1], hot_color);
      nexlcd.setText(*txtmenu_list[LCD_HEIGHT - 1], buffer);

    }
  }

#endif // ADVANCED_PAUSE_FEATURE

#endif
