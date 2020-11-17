load('api_zthing.js');
load('api_zvar.js');
load("api_events.js")

let ZenSensor = {
  _crt: ffi('void *mgos_zsensor_create(char *, int, void*)'),
  _cls: ffi('void mgos_zsensor_close(void *)'),
  _cfgc: ffi('void *mjs_zsensor_cfg_create(int)'),

  _polls: ffi('bool mgos_zsensor_polling_set(void *, int)'),
  _pollp: ffi('bool mgos_zsensor_polling_pause(void *)'),
  _pollr: ffi('bool mgos_zsensor_polling_restart(void *)'),
  _pollc: ffi('bool mgos_zsensor_polling_clear(void *)'),

  _ints: ffi('bool mgos_zsensor_int_set(void *, int, int)'),
  _intp: ffi('bool mgos_zsensor_int_pause(void *)'),
  _intr: ffi('bool mgos_zsensor_int_restart(void *)'),
  _intc: ffi('bool mgos_zsensor_int_clear(void *)'),

  _satg: ffi('void *mgos_zsensor_state_get(void *)'),
  _sng: ffi('char *mgos_zsensor_state_name_get(void *)'),
  _sns: ffi('bool mgos_zsensor_state_name_set(void *, int, char *)'),
  _snbv: ffi('char *mgos_zsensor_state_name_by_val(void *, int)'),
  _snnc: ffi('void mgos_zsensor_state_names_clear(void *)'),

  _shs: ffi('bool mgos_zsensor_state_handler_set(void *, bool (*)(int, void *, userdata), userdata)'),
  _shr: ffi('void mgos_zsensor_state_handler_reset(void *)'),

  _stated: ffi('void *mjs_zsensor_state_descr_get(void)'),
  _stateud: ffi('void *mjs_zsensor_state_upd_descr_get(void)'),

  TYPE: {
    BINARY: (16 | ZenThing.TYPE_SENSOR),
    DECIMAL: (32 | ZenThing.TYPE_SENSOR),
    INTEGER: (48 | ZenThing.TYPE_SENSOR),
  },

  // convert struct mgos_zsensor_state to js object: {
  //   thing: <ZenSensor instance>,
  //   value: <mgos_zvariant *>,
  // }
  parseState: function(state) {
    let sd = this._stated(); 
    let s = s2o(state, sd);
    return {
      thing: ZenThing.getFromHandle(s.handle),
      value: s.value,
    };
  },

  // convert struct mgos_zsensor_state_upd to js object: {
  //   thing: <ZenSensor instance>,
  //   value: <mgos_zvariant *>,
  //   prev_value: <mgos_zvariant *>,
  // }
  parseStateUpd: function(state) {
    let sd = this._stateud();
    let s = s2o(state, sd);
    return {
      thing: ZenThing.getFromHandle(s.handle),
      value: s.value,
      prevValue: s.prev_value,
    };
  },

  // ## **`ZenSensor.create(id, type, cfg)`**
  //
  // Example:
  // ```javascript
  // let cfg = {updatedNotifyMode: ZenSensor.STATE_UPDATED_NOTIFY.IF_CHANGED}
  // let s = ZenSensor.create('sens1', ZenSensor.TYPE.BINARY, cfg);
  // ```
  create: function(id, type, cfg) {
    let cfgo = null;
    if (cfg) {
      cfgo = ZenSensor._cfgc(
        ZenThing._getSafe(cfg.updatedNotifyMode, -1)
      );
      if (cfgo == null) return null;
    }
    // create the handle
    let handle = ZenSensor._crt(id, type, cfgo);
    ZenThing._free(cfgo);
    if (!handle) return null; 
    
    // create the JS instance
    let obj = Object.create(ZenSensor._proto);
    obj.handle = handle;
    obj.id = id;
    obj.type = type;
    
    // raise on-create event
    ZenThing._onCreatePub(obj);

    return obj;
  },

  _proto: {
    handle: null,
    id: null,
    
    close: function() {
      ZenSensor._cls(this.handle);
    },

    setPolling: function(ticks) {
      return ZenSensor._polls(this.handle, ticks);
    },
    pausePolling: function() {
      return ZenSensor._pollp(this.handle);
    },
    restartPolling: function() {
      return ZenSensor._pollr(this.handle);
    },
    clearPolling: function() {
      return ZenSensor._pollc(this.handle);
    },
  
    setInterrupt: function(pin, mode) {
      return ZenSensor._ints(this.handle, pin, mode);
    },
    pauseInterrupt: function() {
      return ZenSensor._intp(this.handle);
    },
    restartInterrupt: function() {
      return ZenSensor._intr(this.handle);
    },
    clearInterrupt: function() {
      return ZenSensor._intc(this.handle);
    },
   
    getState: function() {
      return ZenSensor._satg(this.handle);
    },
    getStateName: function() {
      return ZenSensor._sng(this.handle);
    },
    getStateNameByVal: function(v) {
      return ZenSensor._snbv(this.handle, v);
    },
    clearStateNames: function() {
      return ZenSensor._snnc(this.handle);
    },
    setStateName: function(v, n) {
      return ZenSensor._sns(this.handle, v, n);
    }, 
    setStateHandler: function(handler, userdata) {
      return ZenSensor._shs(this.handle, function(a, d, ud) {
        let state = ZenSensor.parseState(d);
        return ud.h(a, state, ud.ud);
      }, {h:handler, ud:userdata});
    },
    resetStateHandler: function() {
      return ZenSensor._shr(this.handle);
    },

    onStateUpdated: function(handler, userdata) {
      Event.addHandler(ZenThing.EV_STATE_UPDATED, function(ev, evdata, ud) {
        let state = ZenSensor.parseStateUpd(evdata);
        ud.h(state, ud.ud);
      }, {h:handler, ud:userdata});
    },
  },
};