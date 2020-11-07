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
#include "mgos_zsensor_gpio.h"
//Under construction...
```
**JavaScript sample code**
```js
load("api_zsensor_gpio.js")
//Under construction...
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
  enum mgos_zthing_state_updated_notify_mode updated_notify_mode;
};
```
Sensor configuration values (e.g.: used by `mgos_zsensor_create()`).

|Field||
|--|--|
|updated_notify_mode|How the sensor must notify ist state has been updated. See [notify modes](https://github.com/zendiy-mgos/zthing/blob/master/README.md#enum-mgos_zthing_state_updated_notify_mode) for more details.|

**Example** - Create and initialize configuration settings.
```c
// create and initialize cfg using defaults
struct mgos_zsensor_cfg cfg = MGOS_ZSENSOR_CFG;
```

## JS API Reference
*Under construction...*
## Additional resources
Take a look to some other samples or libraries.

|Reference|Type||
|--|--|--|
|[zsensor-gpio](https://github.com/zendiy-mgos/zsensor-gpio)|Library|Mongoose-OS library for attaching a [ZenSensor](https://github.com/zendiy-mgos/zsensor) to a gpio value.|
|[zsensor-mqtt](https://github.com/zendiy-mgos/zsensor-mqtt)|Library|Mongoose-OS library for publishing [ZenSensor](https://github.com/zendiy-mgos/sensor) values as MQTT messages.|
|[zsensor-mqtt-demo](https://github.com/zendiy-mgos/zsensor-mqtt-demo)|Firmware|[Mongoose-OS](https://mongoose-os.com/) demo firmware for publishing pushbutton events as MQTT messages.|