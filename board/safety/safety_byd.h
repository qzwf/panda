#pragma once

#include "safety_declarations.h"

// #define BYD_ATTO3_STEERING_TORQUE       0x1FC // 508
// #define BYD_ATTO3_STEERING_MODULE_ADAS  0x1E2 // 482
// #define BYD_ATTO3_PEDAL                 0x342 // 834
// #define BYD_ATTO3_METER_CLUSTER         0x294 // 660
// #define BYD_ATTO3_STALKS                0x133 // 307
// #define BYD_ATTO3_STEER_MODULE_2        0x11F // 287
// #define BYD_ATTO3_PCM_BUTTONS           0x3B0 // 944
// #define BYD_ATTO3_PEDAL_PRESSED         0x220 // 544
// #define BYD_ATTO3_DRIVE_STATE           0x242 // 578
// #define BYD_ATTO3_ACC_HUD_ADAS          0x32D // 813
// #define BYD_ATTO3_LKAS_HUD_ADAS         0x316 // 790
// #define BYD_ATTO3_ACC_CMD               0x32E // 814
// #define BYD_ATTO3_BSM                   0x418 // 1048
// #define BYD_ATTO3_WHEEL_SPEED           0x122 // 290
// #define BYD_ATTO3_WHEELSPEED_CLEAN      0x1F0 // 496

#define BYD_CANADDR_IPB               0x1F0
#define BYD_CANADDR_ACC_MPC_STATE     0x316
#define BYD_CANADDR_ACC_EPS_STATE     0x318
#define BYD_CANADDR_ACC_HUD_ADAS      0x32D
#define BYD_CANADDR_ACC_CMD           0x32E
#define BYD_CANADDR_PCM_BUTTONS       0x3B0
#define BYD_CANADDR_DRIVE_STATE       0x242
#define BYD_CANADDR_PEDAL             0x342
#define BYD_CANADDR_CARSPEED          0x121

// https://github.com/sunnypilot/panda/blob/1b444df58009896854b01ab62c9a086e87c0ce45/board/safety/safety_byd.h

#define BYD_CANBUS_ESC  0               // ESC Net
#define BYD_CANBUS_MRR  1               // Radar Net
#define BYD_CANBUS_MPC  2               // MPC Net

bool byd_eps_cruiseactivated = false;

const SteeringLimits BYD_STEERING_LIMITS = {
  .max_steer = 300,
  .max_rate_up = 18,
  .max_rate_down = 18,
  .max_rt_delta = 243, // = 18 * 250/20 = 225 + 18
  .max_rt_interval = 250000,
  .max_torque_error = 80,
  .type = TorqueMotorLimited,
};

const CanMsg BYD_TX_MSGS[] = {
  {BYD_CANADDR_ACC_CMD,         0, 8},
  {BYD_CANADDR_ACC_MPC_STATE,   0, 8},
  {BYD_CANADDR_ACC_EPS_STATE,   2, 8},
};

RxCheck byd_han21_rx_checks[] = {
  {.msg = {{BYD_CANADDR_PCM_BUTTONS,      0, 8, .frequency = 20U}, { 0 }, { 0 }}},
  {.msg = {{BYD_CANADDR_ACC_EPS_STATE,    0, 8, .frequency = 50U}, { 0 }, { 0 }}},
  {.msg = {{BYD_CANADDR_CARSPEED,         0, 8, .frequency = 50U}, { 0 }, { 0 }}},
  {.msg = {{BYD_CANADDR_IPB,              0, 8, .frequency = 50U}, { 0 }, { 0 }}},
  {.msg = {{BYD_CANADDR_DRIVE_STATE,      0, 8, .frequency = 50U}, { 0 }, { 0 }}},
};

static void byd_rx_hook(const CANPacket_t *to_push) {
  int bus = GET_BUS(to_push);
  int addr = GET_ADDR(to_push);

  if (bus == BYD_CANBUS_ESC) {
    if (addr == BYD_CANADDR_PEDAL) {
      gas_pressed = (GET_BYTE(to_push, 0) != 0U);
      brake_pressed = (GET_BYTE(to_push, 1) != 0U);

    } else if (addr == BYD_CANADDR_CARSPEED) {
      int speed_raw = (((GET_BYTE(to_push, 1) & 0x0FU) << 8) | GET_BYTE(to_push, 0));
      vehicle_moving = (speed_raw != 0);

    } else if (addr == BYD_CANADDR_ACC_EPS_STATE) {
      byd_eps_cruiseactivated = GET_BIT(to_push, 1U) != 0U; // CruiseActivated
      int torque_motor = (((GET_BYTE(to_push, 2) & 0x0FU) << 8) | GET_BYTE(to_push, 1)); // MainTorque
      if ( torque_motor >= 2048 )
        torque_motor -= 4096;

      update_sample(&torque_meas, torque_motor);

    }

    generic_rx_checks(addr == BYD_CANADDR_ACC_MPC_STATE);

  }else if (bus == BYD_CANBUS_MPC) {
    if (addr == BYD_CANADDR_ACC_CMD) {

    } else if (addr == BYD_CANADDR_ACC_HUD_ADAS) {
      int accstate = ((GET_BYTE(to_push, 2) >> 3) & 0x07U);
      bool cruise_engaged = (accstate == 0x3) || (accstate == 0x5) || byd_eps_cruiseactivated; // 3=acc_active, 5=user force accel
      pcm_cruise_check(cruise_engaged);

      acc_main_on = GET_BIT(to_push, 22U) != 0U;
    }

  }else{
    //do nothing.
  }
}

static bool byd_tx_hook(const CANPacket_t *to_send) {
  bool tx = true;
  int bus = GET_BUS(to_send);

  if (bus == BYD_CANBUS_ESC) {
    //int addr = GET_ADDR(to_send);

    // LKAS torque check, disabled due to incompatible with sp, to be fixed later.
    // if (addr == BYD_CANADDR_ACC_MPC_STATE) {
    //   int desired_torque = ((GET_BYTE(to_send, 3) & 0x07U) << 8U) | GET_BYTE(to_send, 2);
    //   bool steer_req = GET_BIT(to_send, 28U); //LKAS_Active

    //   if ( desired_torque >= 1024 )
    //     desired_torque -= 2048;

    //   if (steer_torque_cmd_checks(desired_torque, steer_req, BYD_STEERING_LIMITS)) {
    //     tx = false;
    //   }
    // }

  }

  return tx;
}

static int byd_fwd_hook(int bus, int addr) {
  int bus_fwd = -1;

  if (bus == BYD_CANBUS_ESC) { // if sent from esc
    if(addr != BYD_CANADDR_ACC_EPS_STATE) {
      bus_fwd = BYD_CANBUS_MPC;
    }

  } else if (bus == BYD_CANBUS_MPC) { // if sent from mpc
    bool block_msg = (addr == BYD_CANADDR_ACC_MPC_STATE) || (addr == BYD_CANADDR_ACC_CMD);
    if (!block_msg) {
      bus_fwd = BYD_CANBUS_ESC;
    }
  }

  return bus_fwd;
}

static safety_config byd_init(uint16_t param) {
  UNUSED(param);
  return BUILD_SAFETY_CFG(byd_han21_rx_checks, BYD_TX_MSGS);
}

const safety_hooks byd_hooks = {
  .init = byd_init,
  .rx = byd_rx_hook,
  .tx = byd_tx_hook,
  .fwd = byd_fwd_hook,
};

