/*
 * Copyright (c) 2020 ZenDIY
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MGOS_ZSENSOR_H_
#define MGOS_ZSENSOR_H_

#include <stdio.h>
#include <stdbool.h>
#include "mgos_zthing.h"

#ifdef __cplusplus
extern "C" {
#endif

// Bit reservation for MGOS_ZTHING_?_SENSOR
// |0|-|-|-|X|X|S|S|
#define MGOS_ZTHING_BINARY_SENSOR (16 | MGOS_ZTHING_SENSOR)   // |0|-|-|1|X|X|S|S|
#define MGOS_ZTHING_DECIMAL_SENSOR (32 | MGOS_ZTHING_SENSOR)  // |0|-|1|0|X|X|S|S|
#define MGOS_ZTHING_INTEGER_SENSOR (48 | MGOS_ZTHING_SENSOR)  // |0|-|1|1|X|X|S|S|
//#define MGOS_ZTHING_?_SENSOR (64 | MGOS_ZTHING_SENSOR)      // |0|1|0|0|X|X|S|S|
//#define MGOS_ZTHING_?_SENSOR (80 | MGOS_ZTHING_SENSOR)      // |0|1|0|1|X|X|S|S|
//#define MGOS_ZTHING_?_SENSOR (96 | MGOS_ZTHING_SENSOR)      // |0|1|1|0|X|X|S|S|
//#define MGOS_ZTHING_?_SENSOR (112 | MGOS_ZTHING_SENSOR)     // |0|1|1|1|X|X|S|S|

enum mgos_zsensor_type {
  ZSENSOR_TYPE_BINARY = MGOS_ZTHING_BINARY_SENSOR,
  ZSENSOR_TYPE_DECIMAL = MGOS_ZTHING_DECIMAL_SENSOR,
  ZSENSOR_TYPE_INTEGER = MGOS_ZTHING_INTEGER_SENSOR,
};

#define MGOS_ZSENSOR_BASE \
  MGOS_ZTHING_BASE

struct mgos_zsensor {
  MGOS_ZSENSOR_BASE
};

#define MGOS_ZSENSOR_CAST(h) ((struct mgos_zsensor *)h)

#define MGOS_ZSENSOR_DEFAULT_POLLING_TICKS 1000 //milliseconds

#define MGOS_ZSENSOR_CFG {                    \
  ZTHING_STATE_UPDATED_NOTIFY_IF_CHANGED }

struct mgos_zsensor_cfg {
  enum mgos_zthing_upd_notify_mode upd_notify_mode;
};

struct mgos_zsensor_state {
  struct mgos_zsensor *handle;
  struct mgos_zvariant *value;
};

struct mgos_zsensor_state_upd {
  struct mgos_zsensor *handle;
  struct mgos_zvariant *value;
  struct mgos_zvariant *prev_value;
};

typedef bool (*mgos_zsensor_state_handler_t)(enum mgos_zthing_state_act act,
                                             struct mgos_zsensor_state *state,
                                             void *user_data);

struct mgos_zsensor *mgos_zsensor_create(const char *id,
                                         enum mgos_zsensor_type sensor_type,
                                         struct mgos_zsensor_cfg *cfg);

bool mgos_zsensor_state_handler_set(struct mgos_zsensor *handle,
                                    mgos_zsensor_state_handler_t handler,
                                    void *user_data);

void mgos_zsensor_state_handler_reset(struct mgos_zsensor *handle);

bool mgos_zsensor_polling_set(struct mgos_zsensor *handle, int polling_ticks);
bool mgos_zsensor_polling_pause(struct mgos_zsensor *handle);
bool mgos_zsensor_polling_restart(struct mgos_zsensor *handle);
bool mgos_zsensor_polling_clear(struct mgos_zsensor *handle);

bool mgos_zsensor_int_set(struct mgos_zsensor *handle,
                          int int_pin, enum mgos_gpio_int_mode int_mode);
bool mgos_zsensor_int_pause(struct mgos_zsensor *handle);
bool mgos_zsensor_int_restart(struct mgos_zsensor *handle);
bool mgos_zsensor_int_clear(struct mgos_zsensor *handle);

void mgos_zsensor_close(struct mgos_zsensor *handle);

struct mgos_zvariant *mgos_zsensor_state_get(struct mgos_zsensor *handle);

const char *mgos_zsensor_state_name_get(struct mgos_zsensor *handle);
const char *mgos_zsensor_state_name_by_val(struct mgos_zsensor *handle, int value);
bool mgos_zsensor_state_name_set(struct mgos_zsensor *handle, int value, const char *name);
void mgos_zsensor_state_names_clear(struct mgos_zsensor *handle);

void mgos_zsensor_cfg_get(struct mgos_zsensor *handle, struct mgos_zsensor_cfg *cfg);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MGOS_ZSENSOR_H_ */