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
 * mmu2.cpp
 *
 * Copyright (C) 2019 Alberto Cotronei @MagoKimbra
 */

#include "../../../MK4duo.h"

#if HAS_MMU2

MMU2 mmu2;

//#define MMU2_DEBUG

#define MMU_TODELAY 100
#define MMU_TIMEOUT 10
#define MMU_CMD_TIMEOUT 60000U 	  // 1 min timeout for mmu commands (except P0)
#define MMU_P0_TIMEOUT   3000U    // timeout for P0 command: 3 seconds

#define MMU_CMD_NONE 0
#define MMU_CMD_T0   0x10
#define MMU_CMD_T1   0x11
#define MMU_CMD_T2   0x12
#define MMU_CMD_T3   0x13
#define MMU_CMD_T4   0x14
#define MMU_CMD_L0   0x20
#define MMU_CMD_L1   0x21
#define MMU_CMD_L2   0x22
#define MMU_CMD_L3   0x23
#define MMU_CMD_L4   0x24
#define MMU_CMD_C0   0x30
#define MMU_CMD_U0   0x40
#define MMU_CMD_E0   0x50
#define MMU_CMD_E1   0x51
#define MMU_CMD_E2   0x52
#define MMU_CMD_E3   0x53
#define MMU_CMD_E4   0x54
#define MMU_CMD_R0   0x60
#define MMU_CMD_F0   0x70
#define MMU_CMD_F1   0x71
#define MMU_CMD_F2   0x72
#define MMU_CMD_F3   0x73
#define MMU_CMD_F4   0x74

#if ENABLED(MMU2_MODE_12V)
  #define MMU_REQUIRED_FW_BUILDNR 132
#else
  #define MMU_REQUIRED_FW_BUILDNR 126
#endif

#define MMU2_NO_TOOL  99
#define MMU_BAUD      115200

#if MMU2_SERIAL > 0
  #if MMU2_SERIAL == 1
    #define mmuSerial Serial1
  #elif MMU2_SERIAL == 2
    #define mmuSerial Serial2
  #elif MMU2_SERIAL == 3
    #define mmuSerial Serial3
  #endif
#else
  #define mmuSerial Serial1
#endif

bool MMU2::enabled, MMU2::ready, MMU2::mmu_print_saved;
uint8_t MMU2::cmd, MMU2::cmd_arg, MMU2::last_cmd, MMU2::extruder;
int8_t MMU2::state = 0;
volatile int8_t MMU2::finda = 1;
volatile bool MMU2::finda_runout_valid;
int16_t MMU2::version = -1, MMU2::buildnr = -1;
millis_s MMU2::last_request_ms, MMU2::next_P0_request_ms;
char MMU2::rx_buffer[16], MMU2::tx_buffer[16];

#if HAS_LCD_MENU

  struct E_Step {
    float extrude;    //!< extrude distance in mm
    float feedRate;   //!< feed rate in mm/s
  };

  static constexpr E_Step ramming_sequence[]        PROGMEM = { MMU2_RAMMING_SEQUENCE };
  static constexpr E_Step load_to_nozzle_sequence[] PROGMEM = { MMU2_LOAD_TO_NOZZLE_SEQUENCE };

#endif // HAS_LCD_MENU

MMU2::MMU2() {
  rx_buffer[0] = '\0';
}

void MMU2::init() {

  set_runout_valid(false);

  #if PIN_EXISTS(MMU2_RST)
    WRITE(MMU2_RST_PIN, HIGH);
    SET_OUTPUT(MMU2_RST_PIN);
  #endif

  mmuSerial.begin(MMU_BAUD);
  extruder = MMU2_NO_TOOL;

  HAL::delayMilliseconds(10);
  reset();
  rx_buffer[0] = '\0';
  state = -1;
}

void MMU2::reset() {
  #if ENABLED(MMU2_DEBUG)
    DEBUG_EM("MMU <= reset");
  #endif

  #if PIN_EXISTS(MMU2_RST)
    WRITE(MMU2_RST_PIN, LOW);
    HAL::delayMilliseconds(20);
    WRITE(MMU2_RST_PIN, HIGH);
  #else
    tx_str_P(PSTR("X0\n")); // Send soft reset
  #endif
}

uint8_t MMU2::get_current_tool() {
  return extruder == MMU2_NO_TOOL ? -1 : extruder;
}

void MMU2::mmu_loop() {

  switch (state) {

    case 0: break;

    case -1:
      if (rx_start()) {
        #if ENABLED(MMU2_DEBUG)
          DEBUG_EM("MMU => 'start'");
          DEBUG_EM("MMU <= 'S1'");
        #endif

        // send "read version" request
        tx_str_P(PSTR("S1\n"));

        state = -2;
      }
      else if (millis() > 3000000) {
        SERIAL_EM("MMU not responding - DISABLED");
        state = 0;
      }
      break;

    case -2:
      if (rx_ok()) {
        sscanf(rx_buffer, "%uok\n", &version);

        #if ENABLED(MMU2_DEBUG)
          DEBUG_EMV("MMU => ", version);
          DEBUG_EM("MMU <= 'S2'");
        #endif

        tx_str_P(PSTR("S2\n")); // read build number
        state = -3;
      }
      break;

    case -3:
      if (rx_ok()) {
        sscanf(rx_buffer, "%uok\n", &buildnr);
        #if ENABLED(MMU2_DEBUG)
          DEBUG_EMV("MMU => ", buildnr);
        #endif

        check_version();

        #if ENABLED(MMU2_MODE_12V)
          #if ENABLED(MMU2_DEBUG)
            DEBUG_EM("MMU <= 'M1'");
          #endif

          tx_str_P(PSTR("M1\n")); // switch to stealth mode
          state = -5;

        #else
          #if ENABLED(MMU2_DEBUG)
            DEBUG_EM("MMU <= 'P0'");
          #endif

          tx_str_P(PSTR("P0\n")); // read finda
          state = -4;
        #endif
      }
      break;

    #if ENABLED(MMU2_MODE_12V)
      case -5:
        // response to M1
        if (rx_ok()) {
          #if ENABLED(MMU2_DEBUG)
            DEBUG_EM("MMU => ok");
          #endif

          #if ENABLED(MMU2_DEBUG)
            DEBUG_EM("MMU <= 'P0'");
          #endif

          tx_str_P(PSTR("P0\n")); // read finda
          state = -4;
        }
        break;
    #endif

    case -4:
      if (rx_ok()) {
        sscanf(rx_buffer, "%hhuok\n", &finda);

        #if ENABLED(MMU2_DEBUG)
          DEBUG_EMV("MMU => ", finda);
          DEBUG_EM("MMU - ENABLED");
        #endif

        enabled = true;
        state = 1;
      }
      break;

    case 1:
      if (cmd) {
        if (WITHIN(cmd, MMU_CMD_T0, MMU_CMD_T4)) {
          // tool change
          int filament = cmd - MMU_CMD_T0;

          #if ENABLED(MMU2_DEBUG)
            DEBUG_EMV("MMU <= T", filament);
          #endif

          tx_printf_P(PSTR("T%d\n"), filament);
          state = 3; // wait for response
        }
        else if (WITHIN(cmd, MMU_CMD_L0, MMU_CMD_L4)) {
          // load
          int filament = cmd - MMU_CMD_L0;

          #if ENABLED(MMU2_DEBUG)
            DEBUG_EMV("MMU <= L", filament);
          #endif

          tx_printf_P(PSTR("L%d\n"), filament);
          state = 3; // wait for response
        }
        else if (cmd == MMU_CMD_C0) {
          // continue loading

          #if ENABLED(MMU2_DEBUG)
            DEBUG_EM("MMU <= 'C0'");
          #endif

          tx_str_P(PSTR("C0\n"));
          state = 3; // wait for response
        }
        else if (cmd == MMU_CMD_U0) {
          // unload current
          #if ENABLED(MMU2_DEBUG)
            DEBUG_EM("MMU <= 'U0'");
          #endif

          tx_str_P(PSTR("U0\n"));
          state = 3; // wait for response
        }
        else if (WITHIN(cmd, MMU_CMD_E0, MMU_CMD_E4)) {
          // eject filament
          int filament = cmd - MMU_CMD_E0;

          #if ENABLED(MMU2_DEBUG)
            DEBUG_EMV("MMU <= E", filament);
          #endif
          tx_printf_P(PSTR("E%d\n"), filament);
          state = 3; // wait for response
        }
        else if (cmd == MMU_CMD_R0) {
          // recover after eject
          #if ENABLED(MMU2_DEBUG)
            DEBUG_EM("MMU <= 'R0'");
          #endif

          tx_str_P(PSTR("R0\n"));
          state = 3; // wait for response
        }
        else if (WITHIN(cmd, MMU_CMD_F0, MMU_CMD_F4)) {
          // filament type
          int filament = cmd - MMU_CMD_F0;
          #if ENABLED(MMU2_DEBUG)
            DEBUG_MV("MMU <= F", filament);
            DEBUG_MV(" ", cmd_arg, DEC);
            DEBUG_MSG("\n");
          #endif

          tx_printf_P(PSTR("F%d %d\n"), filament, cmd_arg);
          state = 3; // wait for response
        }

        last_cmd = cmd;
        cmd = MMU_CMD_NONE;
      }
      else if (expired(&next_P0_request_ms, 300U)) {
        // read FINDA
        tx_str_P(PSTR("P0\n"));
        state = 2; // wait for response
      }
      break;

    case 2:   // response to command P0
      if (rx_ok()) {
        sscanf(rx_buffer, "%hhuok\n", &finda);

        #if ENABLED(MMU2_DEBUG)
          // This is super annoying. Only activate if necessary
          /*
            if (finda_runout_valid) {
              DEBUG_EM("MMU <= 'P0'");
              DEBUG_MSG("MMU => ");
              SERIAL_VAL(finda, DEC);
              DEBUG_MSG("\n");
            }
          */
        #endif

        state = 1;

        if (cmd == 0) ready = true;

        if (!finda && finda_runout_valid) filament_runout();
      }
      else if (expired(&last_request_ms, MMU_P0_TIMEOUT)) // Resend request after timeout (30s)
        state = 1;

      break;

    case 3:   // response to mmu commands
      if (rx_ok()) {
        #if ENABLED(MMU2_DEBUG)
          DEBUG_EM("MMU => 'ok'");
        #endif

        ready = true;
        state = 1;
        last_cmd = MMU_CMD_NONE;
      }
      else if (expired(&last_request_ms, MMU_CMD_TIMEOUT)) {
        // resend request after timeout
        if (last_cmd) {
          #if ENABLED(MMU2_DEBUG)
            DEBUG_EM("MMU retry");
          #endif

          cmd = last_cmd;
          last_cmd = MMU_CMD_NONE;
        }
        state = 1;
      }
      break;
  }
}

/**
 * Check if MMU was started
 */
bool MMU2::rx_start() {
  // check for start message
  if (rx_str_P(PSTR("start\n"))) {
    next_P0_request_ms = millis();
    return true;
  }
  return false;
}

/**
 * Check if the data received ends with the given string.
 */
bool MMU2::rx_str_P(const char* str) {
  uint8_t i = strlen(rx_buffer);

  while (mmuSerial.available()) {
    rx_buffer[i++] = mmuSerial.read();
    rx_buffer[i] = '\0';

    if (i == sizeof(rx_buffer) - 1) {
      #if ENABLED(MMU2_DEBUG)
        DEBUG_EM("rx buffer overrun");
      #endif

      break;
    }
  }

  uint8_t len = strlen_P(str);

  if (i < len) return false;

  str += len;

  while (len--) {
    char c0 = pgm_read_byte(str--), c1 = rx_buffer[i--];
    if (c0 == c1) continue;
    if (c0 == '\r' && c1 == '\n') continue;  // match cr as lf
    if (c0 == '\n' && c1 == '\r') continue;  // match lf as cr
    return false;
  }
  return true;
}

/**
 * Transfer data to MMU, no argument
 */
void MMU2::tx_str_P(const char* str) {
  clear_rx_buffer();
  uint8_t len = strlen_P(str);
  for (uint8_t i = 0; i < len; i++) mmuSerial.write(pgm_read_byte(str++));
  rx_buffer[0] = '\0';
  last_request_ms = millis();
}

/**
 * Transfer data to MMU, single argument
 */
void MMU2::tx_printf_P(const char* format, int argument = -1) {
  clear_rx_buffer();
  uint8_t len = sprintf_P(tx_buffer, format, argument);
  for (uint8_t i = 0; i < len; i++) mmuSerial.write(tx_buffer[i]);
  rx_buffer[0] = '\0';
  last_request_ms = millis();
}

/**
 * Transfer data to MMU, two arguments
 */
void MMU2::tx_printf_P(const char* format, int argument1, int argument2) {
  clear_rx_buffer();
  uint8_t len = sprintf_P(tx_buffer, format, argument1, argument2);
  for (uint8_t i = 0; i < len; i++) mmuSerial.write(tx_buffer[i]);
  rx_buffer[0] = '\0';
  last_request_ms = millis();
}

/**
 * Empty the rx buffer
 */
void MMU2::clear_rx_buffer() {
  while (mmuSerial.available()) mmuSerial.read();
  rx_buffer[0] = '\0';
}

/**
 * Check if we received 'ok' from MMU
 */
bool MMU2::rx_ok() {
  if (rx_str_P(PSTR("ok\n"))) {
    next_P0_request_ms = millis();
    return true;
  }
  return false;
}

/**
 * Check if MMU has compatible firmware
 */
void MMU2::check_version() {
  if (buildnr < MMU_REQUIRED_FW_BUILDNR) {
    SERIAL_SM(ER, "MMU2 firmware version invalid. Required version >= ");
    SERIAL_EV(MMU_REQUIRED_FW_BUILDNR);
    printer.kill(MSG_MMU2_WRONG_FIRMWARE);
  }
}

/**
 * Handle tool change
 */
void MMU2::tool_change(uint8_t index) {

  if (!enabled) return;

  set_runout_valid(false);

  if (index != extruder) {

    printer.keepalive(InHandler);
    stepper.disable_E0();
    lcdui.status_printf_P(0, PSTR(MSG_MMU2_LOADING_FILAMENT), int(index + 1));

    command(MMU_CMD_T0 + index);

    manage_response(true, true);
    printer.keepalive(InHandler);

    command(MMU_CMD_C0);
    extruder = index; // filament change is finished
    tools.active_extruder = 0;

    stepper.enable_E0();

    SERIAL_LMV(ECHO, MSG_ACTIVE_EXTRUDER, int(extruder));

    lcdui.reset_status();
    printer.keepalive(NotBusy);
  }

  set_runout_valid(true);
}

/**
 *
 * Handle special T?/Tx/Tc commands
 *
 * T? Gcode to extrude shouldn't have to follow, load to extruder wheels is done automatically
 * Tx Same as T?, except nozzle doesn't have to be preheated. Tc must be placed after extruder nozzle is preheated to finish filament load.
 * Tc Load to nozzle after filament was prepared by Tx and extruder nozzle is already heated.
 *
 */
void MMU2::tool_change(const char* special) {

  if (!enabled) return;

  #if HAS_LCD_MENU

    set_runout_valid(false);
    printer.keepalive(InHandler);

    switch (*special) {
      case '?': {
        uint8_t index = mmu2_choose_filament();
        while (!hotends[0].wait_for_heating()) HAL::delayMilliseconds(100);
        load_filament_to_nozzle(index);
      } break;

      case 'x': {
        planner.synchronize();
        uint8_t index = mmu2_choose_filament();
        stepper.disable_E0();
        command(MMU_CMD_T0 + index);
        manage_response(true, true);
        command(MMU_CMD_C0);
        mmu_loop();

        stepper.enable_E0();
        extruder = index;
        tools.active_extruder = 0;
      } break;

      case 'c': {
        while (!hotends[0].wait_for_heating()) HAL::delayMilliseconds(100);
        execute_extruder_sequence((const E_Step *)load_to_nozzle_sequence, COUNT(load_to_nozzle_sequence));
      } break;
    }

    printer.keepalive(NotBusy);

    set_runout_valid(true);

  #endif

}

/**
 * Set next command
 */
void MMU2::command(const uint8_t mmu_cmd) {
  if (!enabled) return;
  cmd = mmu_cmd;
  ready = false;
}

/**
 * Wait for response from MMU
 */
bool MMU2::get_response(void) {
  while (cmd != MMU_CMD_NONE) printer.idle();

  while (!ready) {
    printer.idle();
    if (state != 3) break;
  }

  const bool ret = ready;
  ready = false;

  return ret;
}


/**
 * Wait for response and deal with timeout if nexcessary
 */
void MMU2::manage_response(bool move_axes, bool turn_off_nozzle) {

  bool response = false;
  mmu_print_saved = false;
  point_t park_point = NOZZLE_PARK_POINT;
  int16_t resume_hotend_temp;

  while (!response) {

    response = get_response(); // wait for "ok" from mmu

    if (!response) {          // no "ok" was received in reserved time frame, user will fix the issue on mmu unit
      if (!mmu_print_saved) { // first occurence, we are saving current position, park print head in certain position and disable nozzle heater

        planner.synchronize();

        mmu_print_saved = true;

        SERIAL_EM("MMU not responding");

        resume_hotend_temp = hotends[0].target_temperature;
        COPY_ARRAY(mechanics.stored_position[1], mechanics.current_position);

        if (move_axes && mechanics.isHomedAll())
          Nozzle::park(2, park_point /*= NOZZLE_PARK_POINT*/);

        if (turn_off_nozzle) hotends[0].setTarget(0);

        LCD_MESSAGEPGM(MSG_MMU2_NOT_RESPONDING);
        sound.playtone(100, 659);
        sound.playtone(200, 698);
        sound.playtone(100, 659);
        sound.playtone(300, 440);
        sound.playtone(100, 659);

        printer.keepalive(PausedforUser);
      }
    }
    else if (mmu_print_saved) {
      SERIAL_EM("MMU starts responding\n");
      printer.keepalive(InHandler);

      if (turn_off_nozzle && resume_hotend_temp) {
        hotends[0].setTarget(resume_hotend_temp);
        LCD_MESSAGEPGM(MSG_HEATING);
        sound.playtone(200, 40);

        while (!hotends[0].wait_for_heating()) HAL::delayMilliseconds(1000);
      }

      if (move_axes && mechanics.isHomedAll()) {
        LCD_MESSAGEPGM(MSG_MMU2_RESUMING);
        sound.playtone(200, 404);
        sound.playtone(200, 404);

        // Move XY to starting position, then Z
        mechanics.do_blocking_move_to_xy(mechanics.stored_position[1][X_AXIS], mechanics.stored_position[1][Y_AXIS], NOZZLE_PARK_XY_FEEDRATE);

        // Move Z_AXIS to saved position
        mechanics.do_blocking_move_to_z(mechanics.stored_position[1][Z_AXIS], NOZZLE_PARK_Z_FEEDRATE);
      }
      else {
        sound.playtone(200, 404);
        sound.playtone(200, 404);
        LCD_MESSAGEPGM(MSG_MMU2_RESUMING);
      }
    }
  }
}

void MMU2::set_filament_type(uint8_t index, uint8_t filamentType) {
  if (!enabled) return;

  printer.keepalive(InHandler);

  cmd_arg = filamentType;
  command(MMU_CMD_F0 + index);

  manage_response(true, true);

  printer.keepalive(NotBusy);
}

void MMU2::filament_runout() {
  commands.inject_P(PSTR(MMU2_FILAMENT_RUNOUT_SCRIPT));
  planner.synchronize();
}

void MMU2::set_runout_valid(const bool valid) {
  finda_runout_valid = valid;
  #if HAS_FILAMENT_SENSOR
    if (valid) filamentrunout.reset();
  #endif
}

#if HAS_LCD_MENU

  // Load filament into MMU2
  void MMU2::load_filament(uint8_t index) {
    if (!enabled) return;
    command(MMU_CMD_L0 + index);
    manage_response(false, false);
    sound.playtone(200, 404);
  }

  /**
   *
   * Switch material and load to nozzle
   *
   */
  bool MMU2::load_filament_to_nozzle(uint8_t index) {

    if (!enabled) return false;

    if (thermalManager.tooColdToExtrude(ACTIVE_HOTEND)) {
      sound.playtone(200, 404);
      LCD_ALERTMESSAGEPGM(MSG_HOTEND_TOO_COLD);
      return false;
    }
    else {
      printer.keepalive(InHandler);

      command(MMU_CMD_T0 + index);
      manage_response(true, true);
      command(MMU_CMD_C0);
      mmu_loop();

      extruder = index;
      tools.active_extruder = 0;

      load_to_nozzle();

      sound.playtone(200, 404);

      printer.keepalive(NotBusy);
      return true;
    }
  }

  /**
   *
   * Load filament to nozzle of multimaterial printer
   *
   * This function is used only only after T? (user select filament) and M600 (change filament).
   * It is not used after T0 .. T4 command (select filament), in such case, gcode is responsible for loading
   * filament to nozzle.
   */
  void MMU2::load_to_nozzle() {
    if (!enabled) return;
    execute_extruder_sequence((const E_Step *)load_to_nozzle_sequence, COUNT(load_to_nozzle_sequence));
  }

  bool MMU2::eject_filament(uint8_t index, bool recover) {

    if (!enabled) return false;

    if (thermalManager.tooColdToExtrude(ACTIVE_HOTEND)) {
      sound.playtone(200, 404);
      LCD_ALERTMESSAGEPGM(MSG_HOTEND_TOO_COLD);
      return false;
    }

    printer.keepalive(InHandler);
    LCD_MESSAGEPGM(MSG_MMU2_EJECTING_FILAMENT);
    const bool saved_e_relative_mode = printer.axis_relative_modes[E_AXIS];
    printer.axis_relative_modes[E_AXIS] = true;

    stepper.enable_E0();
    mechanics.current_position[E_AXIS] -= MMU2_FILAMENTCHANGE_EJECT_FEED;
    planner.buffer_line(mechanics.current_position[X_AXIS], mechanics.current_position[Y_AXIS], mechanics.current_position[Z_AXIS], mechanics.current_position[E_AXIS], 2500 / 60, tools.active_extruder);
    planner.synchronize();
    command(MMU_CMD_E0 + index);
    manage_response(false, false);

    if (recover)  {
      LCD_MESSAGEPGM(MSG_MMU2_EJECT_RECOVER);
      sound.playtone(200, 404);
      printer.setWaitForUser(true);
      host_action.prompt_do(PROMPT_USER_CONTINUE, PSTR("MMU2 Eject Recover"), PSTR("Continue"));
      while (printer.isWaitForUser()) printer.idle();
      sound.playtone(200, 404);
      sound.playtone(200, 404);

      command(MMU_CMD_R0);
      manage_response(false, false);
    }

    lcdui.reset_status();

    // no active tool
    extruder = MMU2_NO_TOOL;

    set_runout_valid(false);

    sound.playtone(200, 404);

    printer.keepalive(NotBusy);

    printer.axis_relative_modes[E_AXIS] = saved_e_relative_mode;

    stepper.disable_E0();

    return true;
  }

  /**
   *
   * unload from hotend and retract to MMU
   *
   */
  bool MMU2::unload() {

    if (!enabled) return false;

    if (thermalManager.tooColdToExtrude(ACTIVE_HOTEND)) {
      sound.playtone(200, 404);
      LCD_ALERTMESSAGEPGM(MSG_HOTEND_TOO_COLD);
      return false;
    }

    printer.keepalive(InHandler);

    filament_ramming();

    command(MMU_CMD_U0);
    manage_response(false, true);

    sound.playtone(200, 404);

    // no active tool
    extruder = MMU2_NO_TOOL;

    set_runout_valid(false);

    printer.keepalive(NotBusy);

    return true;
  }

  /**
   * Unload sequence to optimize shape of the tip of the unloaded filament
   */
  void MMU2::filament_ramming() {
    execute_extruder_sequence((const E_Step *)ramming_sequence, sizeof(ramming_sequence) / sizeof(E_Step));
  }

  void MMU2::execute_extruder_sequence(const E_Step * sequence, int steps) {

    planner.synchronize();
    stepper.enable_E0();

    const bool saved_e_relative_mode = printer.axis_relative_modes[E_AXIS];
    printer.axis_relative_modes[E_AXIS] = true;

    const E_Step* step = sequence;

    for (uint8_t i = 0; i < steps; i++) {
      const float es = pgm_read_float(&(step->extrude)),
                  fr = pgm_read_float(&(step->feedRate));

      #if ENABLED(MMU2_DEBUG)
        DEBUG_SMV(ECHO, "E step ", es);
        DEBUG_CHR('/');
        DEBUG_EV(fr);
      #endif

      mechanics.current_position[E_AXIS] += es;
      planner.buffer_line(mechanics.current_position[X_AXIS], mechanics.current_position[Y_AXIS], mechanics.current_position[Z_AXIS],
                          mechanics.current_position[E_AXIS], MMM_TO_MMS(fr), tools.active_extruder);
      planner.synchronize();

      step++;
    }

    printer.axis_relative_modes[E_AXIS] = saved_e_relative_mode;

    stepper.disable_E0();
  }

#endif // HAS_LCD_MENU

#endif // PRUSA_MMU2
