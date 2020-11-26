# ZenSensor
## Overview
Mongoose-OS library for ZenSensors ecosystem.
## GET STARTED
Build up your own device in few minutes just starting from the following sample. Start including following libraries into your `mos.yml` file.
```yaml
libs:
  - origin: https://github.com/zendiy-mgos/zsensor-gpio
```
**C/C++ sample code**
```c
#include "mgos.h"
#include "mgos_zsensor.h"

 static bool mg_zsensor_state_h(enum mgos_zthing_state_act act,
                                struct mgos_zsensor_state *state,
                                void *user_data) {
  if (act == MGOS_ZTHING_STATE_GET) {
    // int sens_read = <-- read here sensor value
    mgos_zvariant_integer_set(state->value, sens_read);
    return true;
  }
  return false;
  (void) user_data;
}

void mg_zsensor_state_updated_cb(int ev, void *ev_data, void *userdata) {
  struct mgos_zsensor_state_upd *state = (struct mgos_zsensor_state_upd *)ev_data;
  struct mgos_zsensor *handle = state->handle;

  int cur_val = MGOS_ZVARIANT_PTR_CAST(state->value, int);
  if(mgos_zvariant_is_nav(state->prev_value)) {
    LOG(LL_INFO, ("Updating '%s' value to '%s'(%d)", handle->id, 
      mgos_zsensor_state_name_by_val(handle, cur_val), cur_val));
  } else {
    int prev_val = MGOS_ZVARIANT_PTR_CAST(state->prev_value, int);
    LOG(LL_INFO, ("Updating '%s' value from '%s'(%d) to '%s'(%d)", handle->id, 
      mgos_zsensor_state_name_by_val(handle, prev_val), prev_val,
      mgos_zsensor_state_name_by_val(handle, cur_val), cur_val));
  }

  (void) ev;
  (void) userdata;
}

enum mgos_app_init_result mgos_app_init(void) {
  struct mgos_zsensor *sens1 = mgos_zsensor_create("sens1", ZSENSOR_TYPE_INTEGER, NULL);

  if (sens1 == NULL) return MGOS_APP_INIT_ERROR;

  if (!mgos_zsensor_poll_set(sens1, 2000) || 
      !mgos_zsensor_state_handler_set(sens1, mg_zsensor_state_h, NULL)) {
    return MGOS_APP_INIT_ERROR;
  }

  mgos_zsensor_state_name_set(sens1, 0, "very low");
  mgos_zsensor_state_name_set(sens1, 1, "low");
  mgos_zsensor_state_name_set(sens1, 2, "normal");
  mgos_zsensor_state_name_set(sens1, 3, "high");
  mgos_zsensor_state_name_set(sens1, 4, "very high");

  mgos_event_add_handler(MGOS_EV_ZTHING_STATE_UPDATED, mg_zsensor_state_updated_cb, NULL);

  return MGOS_APP_INIT_SUCCESS;
}
```
**JavaScript sample code**
```js
load("api_zsensor.js")

/* Create sensor using defualt configuration. */   
let sens1 = ZenSensor.create('sens1', ZenSensor.TYPE.INTEGER);

if (sens1) {
  sens1.setStateName(0, 'very low');
  sens1.setStateName(1, 'low');
  sens1.setStateName(2, 'normal');
  sens1.setStateName(3, 'high');
  sens1.setStateName(4, 'very high');

  sens1.setStateHandler(function(act, state, ud) {
    if (act === ZenThing.ACT_STATE_GET) {
      // let sensRead = <-- read here sensor value
      ZenVar.int(state.value, sensRead);
      return true;
    }
    return false;
  }, null);

  sens1.setPoll(2000);

  sens1.onStateUpd(function(state, ud) {
    let sens = state.thing;
    let curVal = ZenVar.int(state.value);
    if (ZenVar.isNaV(state.prevValue)) {
      print('Updating', sens.id,
        "to", sens.getStateNameByVal(curVal), "(", curVal, ")");    
    } else {
      let prevVal = ZenVar.int(state.prevValue);
      print('Updating', sens.id,
        'from', sens.getStateNameByVal(prevVal), "(", prevVal, ")",
        "to", sens.getStateNameByVal(curVal), "(", curVal, ")");
    }
  }, null);
}
```
## C/C++ API Reference
### mgos_zsensor
```c
struct mgos_zsensor {
  char *id;
  int type;
};
```
Sensor handle. You can get a valid handle using `mgos_zsensor_create()`.

|Property||
|--|--|
|id|Handle unique ID.|
|type|Handle type. Possible values: `MGOS_ZTHING_BINARY_SENSOR`, `MGOS_ZTHING_DECIMAL_SENSOR`, `MGOS_ZTHING_INTEGER_SENSOR` or one of the `mgos_zsensor_type` enum values.|

**Example** - Use of handle properties.
```c
struct mgos_zsensor *handle = mgos_zsensor_create("sens-1", ZSENSOR_TYPE_BINARY, NULL);
LOG(LL_INFO, ("ID '%s' detected.", handle->id));
if ((handle->type & MGOS_ZTHING_SENSOR) == MGOS_ZTHING_SENSOR) {
  LOG(LL_INFO, ("Sensor's handle detected."));
}
if (handle->type == ZSENSOR_TYPE_BINARY) {
  LOG(LL_INFO, ("Binary sensor's handle detected."));
}
if (handle->type == MGOS_ZTHING_BINARY_SENSOR) {
  LOG(LL_INFO, ("Binary sensor's handle detected."));
}
```
The console output:
```console
ID 'sens-1' detected.
Sensor's handle detected.
Binary sensor's handle detected.
Binary sensor's handle detected.
```
### mgos_zsensor_cfg
```c
struct mgos_zsensor_cfg {
  enum mgos_zthing_upd_notify_mode upd_notify_mode;
};
```
Sensor configuration values (e.g.: used by `mgos_zsensor_create()`).

|Field||
|--|--|
|upd_notify_mode|One of the [state-updated notify modes](https://github.com/zendiy-mgos/zthing/blob/master/README.md#enum-mgos_zthing_upd_notify_mode) values. This value indicates how the sensor notifies its state has been updated.|

**Example** - Create and initialize configuration settings.
```c
// create and initialize cfg using defaults
struct mgos_zsensor_cfg cfg = MGOS_ZSENSOR_CFG;
```
### mgos_zsensor_create()
```c
struct mgos_zsensor *mgos_zsensor_create(const char *id,
                                         enum mgos_zsensor_type sensor_type,
                                         struct mgos_zsensor_cfg *cfg);
```
Creates and initializes the sensor instance. Returns the instance handle, or `NULL` on error.

|Parameter||
|--|--|
|id|Unique ID.|
|sensor_type|Sensor type.|
|cfg|Optional. [Sensor configuration](https://github.com/zendiy-mgos/zsensor#mgos_zsensor_cfg). If `NULL`, default configuration values are used.|
### mgos_zsensor_()
```c

```
Description.

|Parameter||
|--|--|
|||


## JS API Reference
*Under construction...*
## Additional resources
Take a look to some other samples or libraries.

|Reference|Type||
|--|--|--|
|[zsensor-gpio](https://github.com/zendiy-mgos/zsensor-gpio)|Library|Mongoose-OS library for attaching a [ZenSensor](https://github.com/zendiy-mgos/zsensor) to a GPIO value.|
|[zsensor-mqtt](https://github.com/zendiy-mgos/zsensor-mqtt)|Library|Mongoose-OS library for publishing [ZenSensor](https://github.com/zendiy-mgos/sensor) values as MQTT messages.|
|[zsensor-mqtt-demo](https://github.com/zendiy-mgos/zsensor-mqtt-demo)|Firmware|[Mongoose-OS](https://mongoose-os.com/) demo firmware for publishing pushbutton events as MQTT messages.|