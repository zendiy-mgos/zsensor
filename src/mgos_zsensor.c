#include "mgos.h"
#include "mgos_timers.h"
#include "mgos_zsensor.h"

#ifdef MGOS_HAVE_MJS
#include "mjs.h"
#endif /* MGOS_HAVE_MJS */

static struct mgos_zvariant mg_zsensor_state_nav = MGOS_ZVARIANT_NAV;

struct mg_zsensor_state_handler {
  mgos_zsensor_state_handler_t invoke;
  void *user_data;
};

struct mg_zsensor {
  MGOS_ZSENSOR_BASE
  struct mgos_zsensor_cfg cfg;
  int poll_timer_id;
  int poll_ticks;
  struct mgos_zsensor_int_cfg intr;
  unsigned char is_updating;
  struct mgos_zvariant value;
  struct mg_zsensor_state_handler state_handler;
  struct mg_zsensor_state_name *state_names;
};

#define MG_ZSENSOR_CAST(h) ((struct mg_zsensor *)h)

struct mg_zsensor_state_name {
  int value;
  const char *name;
  struct mg_zsensor_state_name *next;
};

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
    cfg_dest->upd_notify_mode = (cfg_src->upd_notify_mode == -1 ? ZTHING_UPD_NOTIFY_IF_CHANGED : cfg_src->upd_notify_mode);
   } else {
    cfg_dest->upd_notify_mode = ZTHING_UPD_NOTIFY_IF_CHANGED;
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
    handle->poll_timer_id = MGOS_INVALID_TIMER_ID;
    handle->poll_ticks = MGOS_ZTHING_NO_TICKS;
    
    handle->intr.pin = MGOS_ZTHING_NO_PIN;
    handle->intr.mode = MGOS_GPIO_INT_NONE;
    handle->intr.pull_type = MGOS_GPIO_PULL_NONE;
    handle->intr.debounce = 0;

    handle->state_names = NULL;
    handle->is_updating = 0;
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
    free(handle);
  } else {
    LOG(LL_ERROR, ("Error creating '%s'. Memory allocation failed.", id));
  }
  return NULL;
}

static void mg_zsensor_poll_cb(void *);
bool mgos_zsensor_poll_set(struct mgos_zsensor *handle, int poll_ticks) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  if (h != NULL && poll_ticks > 0) {
    if (h->poll_timer_id != MGOS_INVALID_TIMER_ID) mgos_clear_timer(h->poll_timer_id);

    h->poll_timer_id = mgos_set_timer(poll_ticks, MGOS_TIMER_REPEAT, mg_zsensor_poll_cb, handle);
    if (h->poll_timer_id != MGOS_INVALID_TIMER_ID) {
      h->poll_ticks = poll_ticks;
      return true;
    }
  }
  return false;
}

static void mg_zsensor_int_cb(int, void *);
bool mgos_zsensor_int_set(struct mgos_zsensor *handle, int int_pin,
                          enum mgos_gpio_pull_type pull_type, enum mgos_gpio_int_mode int_mode,
                          int debounce_ms) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  if (!h) return false;
  if (h->intr.pin != MGOS_ZTHING_NO_PIN) {
    LOG(LL_ERROR, ("The sensor '%s' is already set to use interrupt on pin %d.", h->id, h->intr.pin));
    return false;
  }
  if (int_pin != MGOS_ZTHING_NO_PIN && int_mode != MGOS_GPIO_INT_NONE) {
    if (mgos_gpio_set_button_handler(int_pin, pull_type, int_mode, debounce_ms, mg_zsensor_int_cb, handle)) {
      h->intr.pin = int_pin;
      h->intr.mode = int_mode;
      h->intr.pull_type = pull_type;
      h->intr.debounce = debounce_ms;
      LOG(LL_INFO, ("Interrupt pin %d successfully set for sensor '%s'.", int_pin, h->id));
      return true;
    }
    LOG(LL_ERROR, ("Unable to set interrupt on pin %d for sensor '%s'.", int_pin, h->id));
  }
  LOG(LL_ERROR, ("Unable to set interrupt. Wrong int_pin (%d) or int_mode (%d) parameters.", int_pin, int_mode));
  return false;
}

bool mgos_zsensor_int_cfg_get(struct mgos_zsensor *handle, struct mgos_zsensor_int_cfg *cfg) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  if (!h || !cfg || (h->intr.pin == MGOS_ZTHING_NO_PIN)) return false;
  cfg->pin = h->intr.pin;
  cfg->mode = h->intr.mode;
  cfg->pull_type = h->intr.pull_type;
  cfg->debounce = h->intr.debounce;
  return true;
}

void mgos_zsensor_cfg_get(struct mgos_zsensor *handle, struct mgos_zsensor_cfg *cfg) {
  if (handle && cfg) {
    struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
    cfg->upd_notify_mode = h->cfg.upd_notify_mode;
  }
}

bool mgos_zsensor_state_handler_set(struct mgos_zsensor *handle,
                                    mgos_zsensor_state_handler_t handler,
                                    void *user_data) {
  if (handle == NULL || handler == NULL) return false;
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  if (h->state_handler.invoke != NULL) {
    LOG(LL_ERROR, ("Error setting value handler for '%s'. Handler already set.", handle->id));
    return false;
  }
  h->state_handler.invoke = handler;
  h->state_handler.user_data = user_data;
  return true;
}

bool mg_zsensor_state_try_update(struct mg_zsensor *handle, bool skip_notify_update) {
  if (!handle || !handle->state_handler.invoke) return false;
  
  ++handle->is_updating;

  struct mgos_zvariant val = MGOS_ZVARIANT_NAV;
  struct mgos_zsensor_state new_state = { MGOS_ZSENSOR_CAST(handle), &val };
  if (!handle->state_handler.invoke(MGOS_ZTHING_STATE_GET, &new_state, handle->state_handler.user_data)) {
    LOG(LL_ERROR, ("Sensor's state handler of '%s' didn't returned a valid value.", handle->id));
    --handle->is_updating;
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
      {
        LOG(LL_ERROR, ("Retrieved value type (%s) is not compatible with '%s' sensor type (%s).",
        mgos_zvariant_type_name(&val), handle->id, mg_zsensor_type_name(handle->type)));
        --handle->is_updating;
        return false;
      }
  }

  bool is_changed = !mgos_zvariant_equals(&val, &handle->value);
  bool notify_update = (!skip_notify_update && ((handle->cfg.upd_notify_mode == ZTHING_UPD_NOTIFY_ALWAIS) ||
                        ((handle->cfg.upd_notify_mode == ZTHING_UPD_NOTIFY_IF_CHANGED) && is_changed)));

  struct mgos_zvariant p_val = MGOS_ZVARIANT_NAV;
  if (notify_update) {
    mgos_zvariant_copy(&handle->value, &p_val);
  }
  /* Update the sensor value, if changed */
  if (is_changed) {
    mgos_zvariant_copy(&val, &handle->value);
    mgos_zvariant_nav_set(&val);
  }
  
  if (notify_update) {
    /* Trigger the STATE_UPDATED event */
    struct mgos_zsensor_state_upd upd_state = { MGOS_ZSENSOR_CAST(handle), &handle->value, &p_val };
    mgos_event_trigger(MGOS_EV_ZTHING_STATE_UPDATED, &upd_state);
    mgos_zvariant_nav_set(&p_val);
    
    if (!is_changed && (handle->cfg.upd_notify_mode != ZTHING_UPD_NOTIFY_ALWAIS)) {
      LOG(LL_DEBUG,("Abnormal: state-updated event raised even if '%s' sensor value is not changed (notify mode %d)",
        handle->id, handle->cfg.upd_notify_mode)); 
    }
  }

  --handle->is_updating;
  return true;
}

struct mgos_zvariant *mgos_zsensor_state_get(struct mgos_zsensor *handle) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  struct mgos_zvariant *res = &mg_zsensor_state_nav;
  if (h) {
    // The value update can be done only on interrupt.
    bool ret_cur_val = (h->poll_ticks == MGOS_ZTHING_NO_TICKS && h->intr.pin != MGOS_ZTHING_NO_PIN);
    if ((h->is_updating > 0) || ret_cur_val) {
      res = &h->value;
    } else if (mg_zsensor_state_try_update(h, true)) {
      res = &h->value;
    }
  }
  return res;
}

const char *mgos_zsensor_state_name_by_val(struct mgos_zsensor *handle, int value) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  if (h) {
    struct mg_zsensor_state_name *vn = h->state_names;
    while (vn != NULL) {
      if (vn->value == value) return vn->name;
      vn = vn->next;
    }
  }
  return NULL;
}

const char *mgos_zsensor_state_name_get(struct mgos_zsensor *handle) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  if (h) {
    if (h->type == ZSENSOR_TYPE_BINARY || h->type == ZSENSOR_TYPE_INTEGER) {
      int value;
      if (mgos_zvariant_integer_get(mgos_zsensor_state_get(handle), &value)) {
        return mgos_zsensor_state_name_by_val(handle, value);
      }
    }
  }
  return NULL;
}

bool mgos_zsensor_state_name_set(struct mgos_zsensor *handle, int value, const char *name) {
  struct mg_zsensor *h = MG_ZSENSOR_CAST(handle);
  if (h && name) {
    if (h->type != ZSENSOR_TYPE_BINARY && h->type != ZSENSOR_TYPE_INTEGER) return false;

    // search for the state value
    struct mg_zsensor_state_name *item = h->state_names;
    while (item && (item->value != value)) { item = item->next; };
    
    if (item == NULL) {
      // state value not found, add a new item
      item = calloc(1, sizeof(struct mg_zsensor_state_name));
      item->value = value;
      item->next = NULL;

      if (!h->state_names) {
        h->state_names = item;
      } else {
        struct mg_zsensor_state_name *last = h->state_names;
        while (last->next != NULL) { last = last->next; };
        last->next = item;
      }
    } else {
      // state value found, update the existing one
      free((char *)item->name);
    }
    
    item->name = strdup(name);
    return true;
  }
  return false;
}

static void mg_zsensor_poll_cb(void *arg) {
  mg_zsensor_state_try_update(MG_ZSENSOR_CAST(arg), false);
}

static void mg_zsensor_update_state_cb(int ev, void *ev_data, void *ud) {
  mg_zsensor_state_try_update(MG_ZSENSOR_CAST(ud), false);
  (void) ev;
  (void) ev_data;
}

static void mg_zsensor_int_cb(int pin, void *arg) {
  if (arg != NULL && MG_ZSENSOR_CAST(arg)->intr.pin == pin) {
    mg_zsensor_state_try_update(MG_ZSENSOR_CAST(arg), false);
  }
}

#ifdef MGOS_HAVE_MJS

struct mgos_zsensor_cfg *mjs_zsensor_cfg_create(enum mgos_zthing_upd_notify_mode upd_notify_mode) {
 struct mgos_zsensor_cfg cfg_src = {
    upd_notify_mode
  };
  struct mgos_zsensor_cfg *cfg_dest = calloc(1, sizeof(struct mgos_zsensor_cfg));
  if (mg_zsensor_cfg_set(&cfg_src, cfg_dest)) return cfg_dest;
  free(cfg_dest);
  return NULL;
}

static const struct mjs_c_struct_member mjs_zsensor_state_descr[] = {
  {"handle", offsetof(struct mgos_zsensor_state, handle), MJS_STRUCT_FIELD_TYPE_VOID_PTR, NULL}, 
  {"value", offsetof(struct mgos_zsensor_state, value), MJS_STRUCT_FIELD_TYPE_VOID_PTR, NULL},
  {NULL, 0, MJS_STRUCT_FIELD_TYPE_INVALID, NULL},
};

const struct mjs_c_struct_member *mjs_zsensor_state_descr_get(void) {
  return mjs_zsensor_state_descr;
};

static const struct mjs_c_struct_member mjs_zsensor_state_upd_descr[] = {
  {"handle", offsetof(struct mgos_zsensor_state_upd, handle), MJS_STRUCT_FIELD_TYPE_VOID_PTR, NULL}, 
  {"value", offsetof(struct mgos_zsensor_state_upd, value), MJS_STRUCT_FIELD_TYPE_VOID_PTR, NULL},
  {"prev_value", offsetof(struct mgos_zsensor_state_upd, prev_value), MJS_STRUCT_FIELD_TYPE_VOID_PTR, NULL},
  {NULL, 0, MJS_STRUCT_FIELD_TYPE_INVALID, NULL},
};

const struct mjs_c_struct_member *mjs_zsensor_state_upd_descr_get(void) {
  return mjs_zsensor_state_upd_descr;
};

#endif /* MGOS_HAVE_MJS */


bool mgos_zsensor_init() {
  return true;
}