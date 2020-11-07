#include <stdarg.h>
#include "mgos.h"
#include "mgos_timers.h"
#include "mgos_zsensor.h"

#ifdef MGOS_HAVE_MJS
#include "mjs.h"
#endif /* MGOS_HAVE_MJS */

static struct mgos_zvariant mg_zsensor_value_nav = MGOS_ZVARIANT_NAV;

struct mg_zsensor_value_handler {
  mgos_zsensor_value_handler_t invoke;
  void *user_data;
};

struct mg_zsensor {
  MGOS_ZSENSOR_BASE
  struct mgos_zsensor_cfg cfg;
  int polling_timer_id;
  int polling_ticks;
  int interrupt_pin;
  int interrupt_mode;
  char **str_states;
  int str_states_count;
  struct mgos_zvariant value;
  struct mg_zsensor_value_handler value_handler; 
};

#define MG_ZSENSOR_CAST(h) ((struct mg_zsensor *)h)

const char *mg_zsensor_type_name(enum mgos_zsensor_type t) {
  switch (t)
  {
    case ZSENSOR_TYPE_BINARY:
      return "binary";
    case ZSENSOR_TYPE_DECIMAL:
      return "decimal";
    case ZSENSOR_TYPE_INTEGER:
      return "integer";
    default:
      break;
  }
  return "undefined";
}

bool mg_zsensor_cfg_set(struct mgos_zsensor_cfg *cfg_src,
                        struct mgos_zsensor_cfg *cfg_dest) {
  if (!cfg_dest) return false;
  if (cfg_src != NULL) {
    cfg_dest->updated_notify_mode = (cfg_src->updated_notify_mode == -1 ? ZTHING_STATE_UPDATED_NOTIFY_IF_CHANGED : cfg_src->updated_notify_mode);
   } else {
    cfg_dest->updated_notify_mode = ZTHING_STATE_UPDATED_NOTIFY_IF_CHANGED;
  }
  return true;
}

static void mg_zsensor_update_state_cb(int, void *, void *);
struct mgos_zsensor *mgos_zsensor_create(const char *id,
                                         enum mgos_zsensor_type sensor_type,
                                         struct mgos_zsensor_cfg *cfg) {
  if (id == NULL) return NULL;
  struct mg_zsensor *handle = calloc(1, sizeof(struct mg_zsensor));
  if (handle != NULL) {
    handle->id = strdup(id);
    handle->type = sensor_type;
    handle->polling_timer_id = MGOS_INVALID_TIMER_ID;
    handle->polling_ticks = MGOS_ZTHING_NO_TICKS;
    handle->interrupt_pin = MGOS_ZTHING_NO_PIN;
    handle->interrupt_mode = MGOS_GPIO_INT_NONE;
    handle->str_states = NULL;
    handle->str_states_count = 0;
    mgos_zvariant_nav_set(&handle->value);

    if (mg_zsensor_cfg_set(cfg, &handle->cfg)) {
      if (mgos_zthing_register(MGOS_ZTHING_CAST(handle))) {
        /* Trigger the CREATED event */
        mgos_event_trigger(MGOS_EV_ZTHING_CREATED, handle);

        mgos_event_add_handler(MGOS_EV_ZTHING_UPDATE_STATE,
          mg_zsensor_update_state_cb, handle);

        LOG(LL_INFO, ("Sensor '%s' successfully created (type %d).", handle->id, handle->type));
        return MGOS_ZSENSOR_CAST(handle);
      }
      LOG(LL_ERROR, ("Error creating '%s'. Handle registration failed.", id));
    }
    mgos_zsensor_close(MGOS_ZSENSOR_CAST(handle));
  } else {
    LOG(LL_ERROR, ("Error creating '%s'. Memory allocation failed.", id));
  }
  return NULL;
}

static void mg_zsensor_polling_cb(void *);
bool mgos_zsensor_polling_set(struct mgos_zsensor *handle, int polling_ticks) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  if (h != NULL && polling_ticks > 0) {
    h->polling_ticks = polling_ticks;
    if (mgos_zsensor_polling_restart(handle)) return true;
    h->polling_ticks = MGOS_ZTHING_NO_TICKS;
  }
  return false;
}

bool mgos_zsensor_polling_pause(struct mgos_zsensor *handle) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  if (!h) return false;
  if (h->polling_timer_id != MGOS_INVALID_TIMER_ID) {
    mgos_clear_timer(h->polling_timer_id);
    h->polling_timer_id = MGOS_INVALID_TIMER_ID;
  }
  return true;
}

bool mgos_zsensor_polling_restart(struct mgos_zsensor *handle) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  if (h == NULL || h->polling_timer_id != MGOS_INVALID_TIMER_ID) return false;
  h->polling_timer_id = mgos_set_timer(h->polling_ticks,
    MGOS_TIMER_REPEAT, mg_zsensor_polling_cb, handle);
  return (h->polling_timer_id != MGOS_INVALID_TIMER_ID);
}

bool mgos_zsensor_polling_clear(struct mgos_zsensor *handle) {
  if (!mgos_zsensor_polling_pause(handle)) return false;
  MG_ZSENSOR_CAST(handle)->polling_ticks = MGOS_ZTHING_NO_TICKS;
  return true;
}

static void mg_zsensor_interrupt_cb(int, void *);
bool mgos_zsensor_int_set(struct mgos_zsensor *handle, int int_pin, enum mgos_gpio_int_mode int_mode) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  if (!h) return false;
  if (h->interrupt_pin != MGOS_ZTHING_NO_PIN) {
    return false;
  }
  if (int_pin == MGOS_ZTHING_NO_PIN && int_mode == MGOS_GPIO_INT_NONE) {
    if (mgos_gpio_set_int_handler(int_pin, int_mode, mg_zsensor_interrupt_cb, handle)) {
      h->interrupt_pin = int_pin;
      h->interrupt_mode = int_mode;
      return true;
    }
  }
  return false;
}

bool mgos_zsensor_int_pause(struct mgos_zsensor *handle) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  return (h ? mgos_gpio_disable_int(h->interrupt_pin) : false);
}

bool mgos_zsensor_int_restart(struct mgos_zsensor *handle) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  return (h ? mgos_gpio_enable_int(h->interrupt_pin) : false);
}

bool mgos_zsensor_int_clear(struct mgos_zsensor *handle) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  if (!h) return false;
  if (h->interrupt_pin != MGOS_ZTHING_NO_PIN) {
    if (!mgos_gpio_disable_int(h->interrupt_pin)) return false;
    mgos_gpio_remove_int_handler(h->interrupt_pin, NULL, NULL);
    h->interrupt_pin = MGOS_ZTHING_NO_PIN;
  }
  h->interrupt_mode = MGOS_GPIO_INT_NONE;
  return true;
}

void mgos_zsensor_close(struct mgos_zsensor *handle) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  if (h) {
    mgos_zsensor_polling_clear(handle);
    mgos_zsensor_int_clear(handle);

    for (int i = 0; i < h->str_states_count; ++i) {
      free(h->str_states[i]);
    }
    free(h->str_states);
    h->str_states_count = 0;

    mgos_zsensor_value_handler_reset(handle);
  }
  mgos_zthing_close(MGOS_ZTHING_CAST(h));
}

void mgos_zsensor_cfg_get(struct mgos_zsensor *handle, struct mgos_zsensor_cfg *cfg) {
  if (handle && cfg) {
    struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
    cfg->updated_notify_mode = h->cfg.updated_notify_mode;
  }
}

bool mgos_zsensor_value_handler_set(struct mgos_zsensor *handle,
                                    mgos_zsensor_value_handler_t handler,
                                    void *user_data) {
  if (handle == NULL || handler == NULL) return false;
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  if (h->value_handler.invoke != NULL) {
    LOG(LL_ERROR, ("Error setting value handler for '%s'. Handler already set.", handle->id));
    return false;
  }
  h->value_handler.invoke = handler;
  h->value_handler.user_data = user_data;
  return true;
}

void mgos_zsensor_value_handler_reset(struct mgos_zsensor *handle) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  if (h != NULL) {
    h->value_handler.invoke = NULL;
    h->value_handler.user_data = NULL;
  }
}

bool mg_zsensor_value_try_update(struct mg_zsensor *handle, bool skip_notify_update) {
  if (!handle || !handle->value_handler.invoke) return false;
  if (handle->polling_ticks == MGOS_ZTHING_NO_TICKS && 
      handle->interrupt_pin != MGOS_ZTHING_NO_PIN) {
    // The value update can be done only on interrupt.
    return true;
  }

  struct mgos_zvariant val = MGOS_ZVARIANT_NAV;
  if (!handle->value_handler.invoke(MGOS_ZSENSOR_CAST(handle), &val, handle->value_handler.user_data)) {
    return false;
  }

  // Check if variant type is compatible with
  // the sensor type
  switch (handle->type)
  {
    case ZSENSOR_TYPE_BINARY:
      if (val.type == ZVARIANT_BOOL) break;
    case ZSENSOR_TYPE_INTEGER:
      if (val.type == ZVARIANT_INT || val.type == ZVARIANT_LONG) break;
    case ZSENSOR_TYPE_DECIMAL:
      if (val.type == ZVARIANT_FLOAT || val.type == ZVARIANT_DOUBLE) break;
    default:
      LOG(LL_ERROR, ("Retrieved value type (%s) is not compatible with '%s' sensor type (%d).",
        mgos_zvariant_type_name(&val), handle->id, mg_zsensor_type_name(handle->type)));
      return false;
  }
  
  bool is_changed = !mgos_zvariant_equals(&val, &handle->value); 
  bool notify_update = ((handle->cfg.updated_notify_mode == ZTHING_STATE_UPDATED_NOTIFY_ALWAIS) ||
                        ((handle->cfg.updated_notify_mode == ZTHING_STATE_UPDATED_NOTIFY_IF_CHANGED) && is_changed));

  /* Update the sensor value, if changed */
  if (is_changed) {
    if(!mgos_zvariant_copy(&val, &handle->value)) return false;
  }
  
  if (!skip_notify_update && notify_update) {
    struct mgos_zsensor_state state = { MGOS_ZSENSOR_CAST(handle) };
    if (mgos_zvariant_copy(&handle->value, &state.value)) {
      state.str_value = mgos_zsensor_str_value_get(state.handle);
      /* Trigger the STATE_UPDATED event */
      mgos_event_trigger(MGOS_EV_ZTHING_STATE_UPDATED, &state);
    }
  }

  return true;
}

struct mgos_zvariant *mgos_zsensor_value_get(struct mgos_zsensor *handle) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  if (!h || !mg_zsensor_value_try_update(h, true)) return &mg_zsensor_value_nav;
  return &h->value;
}

const char *mgos_zsensor_str_value_get(struct mgos_zsensor *handle) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  int idx = -1;
  if (h) {
    if (h->type == ZSENSOR_TYPE_BINARY ||
        h->type == ZSENSOR_TYPE_INTEGER) {
      mgos_zvariant_integer_get(mgos_zsensor_value_get(handle), &idx);
    }
  }
  return (idx != -1 && idx < h->str_states_count ? h->str_states[idx] : NULL);
}

bool mgos_zsensor_str_values_set(struct mgos_zsensor *handle, ...) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  if (h) {
    if (h->type != ZSENSOR_TYPE_BINARY &&
        h->type != ZSENSOR_TYPE_INTEGER) {
      return false;
    }
    va_list vl;
    va_start(vl, handle);
    h->str_states_count = (h->type == ZSENSOR_TYPE_BINARY ? 2 : va_arg(vl, int));
    h->str_states = malloc(sizeof(char *) * h->str_states_count);
    for (int i = 0; i < h->str_states_count; ++i) {
      h->str_states[i] = strdup(va_arg(vl, char*));
    }
    va_end(vl);
    return true;
  }
  return false;
}

static void mg_zsensor_polling_cb(void *arg) {
  mg_zsensor_value_try_update(MG_ZSENSOR_CAST(arg), false);
}

static void mg_zsensor_update_state_cb(int ev, void *ev_data, void *ud) {
  mg_zsensor_value_try_update(MG_ZSENSOR_CAST(ud), false);
  (void) ev;
  (void) ev_data;
}

static void mg_zsensor_interrupt_cb(int pin, void *arg) {
  if (arg != NULL && MG_ZSENSOR_CAST(arg)->interrupt_pin == pin) {
    mg_zsensor_value_try_update(MG_ZSENSOR_CAST(arg), false);
  }
}

#ifdef MGOS_HAVE_MJS

struct mgos_zsensor_cfg *mjs_zsensor_cfg_create(enum mgos_zthing_notify_update_state_mode updated_notify_mode,
                                                const char *binary_state_true,
                                                const char *binary_state_false) {
 struct mgos_zsensor_cfg cfg_src = {
    updated_notify_mode,
    binary_state_true,
    binary_state_false
  };
  struct mgos_zsensor_cfg *cfg_dest = calloc(1, sizeof(struct mgos_zsensor_cfg));
  if (mg_zsensor_cfg_set(&cfg_src, cfg_dest)) return cfg_dest;
  free(cfg_dest);
  return NULL;
}

#endif /* MGOS_HAVE_MJS */


bool mgos_zsensor_init() {
  return true;
}