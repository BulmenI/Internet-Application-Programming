#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

const int ADDR_OUT_XLSB   = 0xF8;
const int ADDR_OUT_LSB    = 0xF7;
const int ADDR_OUT_MSB    = 0xF6;
const int ADDR_CTRL_MEAS  = 0xF4;
const int ADDR_SOFT_RESET = 0xE0;
const int ADDR_ID         = 0xD0;
const int ADDR_CALIB0     = 0xAA;
const int ADDR_CALIB21    = 0xBF;

const int CMD_READTEMP  = 0x2E;
const int CMD_READPRESS = 0x34;

const int16_t  AC1 = 408;
const int16_t  AC2 = -72;
const int16_t  AC3 = -14383;
const uint16_t AC4 = 32741;
const uint16_t AC5 = 32757;
const uint16_t AC6 = 23153;
const int16_t  B1  = 6190;
const int16_t  B2  = 4;
const int16_t  MB  = -32768;
const int16_t  MC  = -8711;
const int16_t  MD  = 2868;

typedef struct {
  uint32_t temp_attr;
  uint32_t press_attr;

  uint8_t out_xlsb;
  uint8_t out_lsb;
  uint8_t out_msb;
  uint8_t ctrl_meas;
  uint8_t soft_reset;
  uint8_t id;
  uint8_t calib[22];

  uint8_t op_target_addr;

  int32_t B5;

  uint32_t temp_timer;
  uint32_t press_timer;
} chip_state_t;

static bool on_i2c_connect(void *user_data, uint32_t address, bool connect);
static uint8_t on_i2c_read(void *user_data);
static bool on_i2c_write(void *user_data, uint8_t data);
static void on_temp_timer_event(void *user_data);
static void on_press_timer_event(void *user_data);

void chip_init() {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  memset(chip, 0, sizeof(chip_state_t));
  const uint16_t calib_vals[11] = {AC1, AC2, AC3, AC4, AC5, AC6, B1, B2, MB, MC, MD};
  for (int i = 0; i < sizeof(calib_vals); i++) {
    chip->calib[2*i] = (calib_vals[i] >> 8) & 0xFF;
    chip->calib[2*i + 1] = calib_vals[i] & 0xFF;
  }
  chip->id = 0x55;
  chip->out_msb = 0x80;

  chip->temp_attr = attr_init_float("temperature", 20.0);
  chip->press_attr = attr_init("pressure", 101325);

  const timer_config_t temp_timer_config = {
    .user_data = chip,
    .callback = on_temp_timer_event,
  };
  chip->temp_timer = timer_init(&temp_timer_config);

  const timer_config_t press_timer_config = {
    .user_data = chip,
    .callback = on_press_timer_event,
  };
  chip->press_timer = timer_init(&press_timer_config);

  const i2c_config_t i2c_config = {
    .user_data = chip,
    .address = 0x77,
    .scl = pin_init("SCL", INPUT),
    .sda = pin_init("SDA", INPUT),
    .connect = on_i2c_connect,
    .read = on_i2c_read,
    .write = on_i2c_write,
  };
  i2c_init(&i2c_config);
}


bool on_i2c_connect(void *user_data, uint32_t address, bool connect) {
  return true; /* Ack */
}

uint8_t on_i2c_read(void *user_data) {
  chip_state_t *chip = user_data;
  
  uint8_t addr = chip->op_target_addr;
  if (addr == ADDR_OUT_MSB) {
    chip->op_target_addr++; // allow 16bit read
  } else if (addr >= ADDR_CALIB0 && addr < ADDR_CALIB21 && addr % 2 == 0) {
    chip->op_target_addr++; // allow 16bit read
  } else {
    chip->op_target_addr = 0;
  }

  switch (addr) {
    case ADDR_OUT_XLSB:
      // printf("REG_READ: XLSB=%x\n", chip->out_xlsb);
      return chip->out_xlsb;
    case ADDR_OUT_LSB:
      // printf("REG_READ: LSB=%x\n", chip->out_lsb);
      return chip->out_lsb;
    case ADDR_OUT_MSB:
      // printf("REG_READ: MSB=%x\n", chip->out_msb);
      return chip->out_msb;
    case ADDR_CTRL_MEAS:
      // printf("REG_READ: CTRL_MEAS=%x\n", chip->ctrl_meas);
      return chip->ctrl_meas;
    case ADDR_SOFT_RESET:
      // printf("REG_READ: SOFT_RESET=%x\n", chip->soft_reset);
      return chip->soft_reset; // ?
    case ADDR_ID:
      // printf("REG_READ: ID=%x\n", chip->id);
      return chip->id;
    default:
      if (addr >= ADDR_CALIB0 && addr <= ADDR_CALIB21) {
        // printf("REG_READ: CALIB[%d]=%x\n", addr - ADDR_CALIB0, chip->calib[addr - ADDR_CALIB0]);
        return chip->calib[addr - ADDR_CALIB0];
      } else {
        // printf("REG_READ: ??????\n");
        return 0; // unknown
      }
  }
}

static bool on_i2c_recvd_cmd(chip_state_t *chip, uint8_t cmd) {
  chip->op_target_addr = 0;

  if (cmd == CMD_READTEMP) {
    // printf("Starting simulated temperature read\n");
    timer_start(chip->temp_timer, 4 * 1000, false); // 4ms
  } else if (cmd == (CMD_READPRESS + (0 << 6))) {
    // printf("Starting simulated pressure read (ultra-low power)\n");
    timer_start(chip->press_timer, 4 * 1000, false); // 4ms
  } else if (cmd == (CMD_READPRESS + (1 << 6))) {
    // printf("Starting simulated pressure read (standard)\n");
    timer_start(chip->press_timer, 7 * 1000, false); // 7ms
  } else if (cmd == (CMD_READPRESS + (2 << 6))) {
    // printf("Starting simulated pressure read (high-res)\n");
    timer_start(chip->press_timer, 13 * 1000, false); // 13ms
  } else if (cmd == (CMD_READPRESS + (3 << 6))) {
    // printf("Starting simulated pressure read (ultra high-res)\n");
    timer_start(chip->press_timer, 25 * 1000, false); // 25ms
  } else {
    // unsupported command/combo
    return false;
  }

  chip->ctrl_meas = cmd;
  return true;
}

bool on_i2c_write(void *user_data, uint8_t data) {
  chip_state_t *chip = user_data;
 
  if (chip->op_target_addr == ADDR_CTRL_MEAS) {
    return on_i2c_recvd_cmd(chip, data);
  } else if (chip->op_target_addr == ADDR_SOFT_RESET) {
    // printf("attempted soft reset: not implemented, nacking\n");
    chip->op_target_addr = 0;
    return false; // not implemented
  } else  {
    chip->op_target_addr = data; // attempting reg read
    return true; // ACK
  }
}

static void on_temp_timer_event(void *user_data) {
  chip_state_t *chip = user_data;

  int32_t T = round(attr_read_float(chip->temp_attr) * 10); 
  double B5 = T * pow(2, 4) - 8;
  double UTd = pow((double)MD + B5, 2) - (double)8192.0 * MC;
  UTd *= pow(AC5, 2);
  UTd = sqrt(UTd) - AC5*MD + AC5*B5;
  UTd *= 16384.0;
  UTd /= pow(AC5, 2);
  UTd += AC6;

  chip->B5 = B5;

  int16_t UT = round(UTd);
  chip->out_msb = (UT >> 8) & 0xFF;
  chip->out_lsb = UT & 0xFF;
  chip->ctrl_meas &= ~((uint8_t)1 << 5); 
  // printf("Wrote %x to output regs (temperature)\n", UT);
}
static void on_press_timer_event(void *user_data) {
  chip_state_t *chip = user_data;

  uint8_t oss = chip->ctrl_meas >> 6;
  int32_t B6 = chip->B5 - 4000;
  
  int32_t X1_3 = (B2 * (B6 * B6 / 4096)) / 2048;
  int32_t X2_3 = AC2 * B6 / 2048;
  int32_t X3_3 = X1_3 + X2_3;
  int32_t B3 = (((AC1*4 + X3_3) << oss) + 2)/ 4;
  
  int32_t X1_4 = AC3 * B6 / 8192;
  int32_t X2_4 = (B1 * (B6 * B6 / 4096)) / 65536;
  int32_t X3_4 = ((X1_4 + X2_4) + 2) / 4;
  uint32_t B4 = AC4 * (uint32_t)(X3_4 + 32768) / 32768;
  
  double p = attr_read(chip->press_attr); // Pa
  double p_2 = sqrt((double)194432.0 * p + 1084090937729.0) - 1041219.0;
  p_2 *= 16384.0;
  p_2 /= 1519.0;

  double B7_2 = p_2 / 2 * B4;
  double UPd = B7_2 / (double)(50000 >> oss) + B3;

  uint32_t UP = ((uint32_t) round(UPd)) << (8 - oss);
  chip->out_msb = (UP >> 16) & 0xFF;
  chip->out_lsb = (UP >> 8) & 0xFF;
  chip->out_xlsb = UP & 0xFF;
  chip->ctrl_meas &= ~((uint8_t)1 << 5); // clear sco bit
  // printf("Wrote %x to output regs (pressure)\n", UP);
}
