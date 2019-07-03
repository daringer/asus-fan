#include "mach_base.h"
#include "msg.h"

// Machine definition for UX410U(AK) / UX3410U(AK) laptops
// Some machine specific registers:
// _SB.PCI0.LPCB.EC0.WRAM 0x0521 0xc5 # full fan speed, locks register 0x97;
// .TACH method shows current fan speed _SB.PCI0.LPCB.EC0.WRAM 0x0521 0x85 #
// back to automatic fan speed control _SB.PCI0.LPCB.EC0.WRAM 0x0521 0x35 #
// manual fan control. Fan speed can be controlled by .ST84 call.
// _SB.PCI0.LPCB.EC0.ST84 Arg0 Arg1 # Arg0 - fan id (0x00 for CPU fan). Arg1 -
// speed 0x00 - 0xff See also: https://github.com/daringer/asus-fan/issues/44

// must forward declare functions for machine control struct
static bool is_machine(const char *system_vendor, const char *product_name);
static int fan_get_cur_control_state(int fan, unsigned long *state);
static int fan_set_cur_control_state(int fan, unsigned long state);
static int fan_set_speed(int fan, int speed);
static int fan_rpm(int fan);
static int fan_get_max_speed(int fan, unsigned long *state);
static int fan_set_max_speed(int fan, unsigned long state, bool reset);
static int fan_set_auto();
static int temp_get(int sensor);
static int control_func(void *data);
static void initialize();

static struct asus_machine_control machine_default = {is_machine,
                                                      initialize,
                                                      fan_get_cur_control_state,
                                                      fan_set_cur_control_state,
                                                      fan_set_speed,
                                                      fan_rpm,
                                                      fan_get_max_speed,
                                                      fan_set_max_speed,
                                                      fan_set_auto,
                                                      temp_get,
                                                      NULL, /*control_func,*/
                                                      "UX410UAK",
                                                      NullAsusFanData};

// Helpers for ACPI calls
static struct acpi_object_list params;
static union acpi_object args[2];

// Machine-specific implementations

static bool is_machine(const char *system_vendor, const char *product_name) {
  return strcmp(system_vendor, "ASUSTeK COMPUTER INC.") == 0 &&
         strcmp(product_name, "UX410UAK") == 0;
}

static int fan_get_cur_state(int fan, unsigned long *state) {
  /* very nasty, but (by now) the only idea I had to calculate the pwm value
  from the measured pwms
  * => heat up the notebook
  * => reduce maxumum fan speed
  * => rpms are still updated and you know the pwm value => Mapping Table
  * => do a regression
  * => =RPM*RPM*0,0000095+0,01028*RPM+26,5
  RPMs	PWM
  3640	190
  3500	180
  3370	170
  3240	160
  3110	150
  2960	140
  2800	130
  2640	120
  2470	110
  2290	100
  2090	90
  1890	80
  1660	70
  1410	60
  1110	50
  950	45
  790	40
*/
  int rpm = fan_rpm(fan);
  dbg_msg("fan-id: %d | get RPM", fan);
  if (fan_data.fan_manual_mode[fan]) {
    *state = fan_data.fan_state[fan];
  } else {
    if (rpm == 0) {
      *state = 0;
      return 0;
    }
    *state = rpm * rpm * 100 / 10526316 + rpm * 1000 / 97276 + 26;
    // ensure state is within a valid range
    if (*state > 255) {
      *state = 0;
    }
  }
  return 0;
}

static int fan_set_cur_control_state(int fan, unsigned long state) {
  dbg_msg("fan-id: %d | set state: %d", fan, state);
  // catch illegal state set
  if (state > 255) {
    warn_msg("set pwm", "illegal value provided: %d ", fan, state);
    return 1;
  }
  fan_data.fan_state[fan] = state;
  fan_data.fan_manual_mode[fan] = true;
  return fan_set_speed(fan, state);
}

static int fan_get_cur_control_state(int fan, int *state) {
  dbg_msg("fan-id: %d | get control state", fan);
  *state = fan_data.fan_manual_mode[fan];
  return 0;
}

static int fan_set_cur_control_state(int fan, int state) {
  dbg_msg("fan-id: %d | set control state: %d", fan, state);
  if (state == 0) {
    return fan_set_auto();
  }
  return 0;
}

static int fan_set_speed(int fan, int speed) {
  union acpi_object args[2];
  unsigned long long value;
  acpi_status ret;

  dbg_msg("fan-id: %d | set speed: %d", fan, speed);
  // set speed to 'speed' for given 'fan'-index
  // -> automatically switch to manual mode!
  params.count = ARRAY_SIZE(args);
  params.pointer = args;
  args[0].type = ACPI_TYPE_INTEGER;
  args[0].integer.value = 0x521;
  args[1].type = ACPI_TYPE_INTEGER;
  args[1].integer.value = 0x35;
  ret =
      acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.WRAM", &params, &value);
  if (ret != AE_OK) {
    err_msg("fan_set_speed", "failed to write WRAM, errcode: %d", ret);
    return ret;
  }
  // target fan speed
  // - between 0x00 and MAX (0 - MAX)
  // - 'MAX' is usually 0xFF (255)
  // - should be getable with fan_get_max_speed()
  args[0].integer.value = fan;
  args[1].integer.value = speed;
  ret =
      acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.ST84", &params, &value);
  if (ret != AE_OK) {
    err_msg("fan_set_speed", "failed to write ST84, errcode: %d", ret);
  }
  return ret;
}

static int fan_rpm(int fan) {
  struct acpi_object_list params;
  union acpi_object args[1];
  unsigned long long value;
  acpi_status ret;

  dbg_msg("fan-id: %d | get RPM", fan);
  // fan does not report during manual speed setting - so fake it!
  if (fan_data.fan_manual_mode[fan]) {
    value =
        fan_data.fan_state[fan] * fan_data.fan_state[fan] * 1000 / -16054 +
        fan_data.fan_state[fan] * 32648 / 1000 - 365;
    dbg_msg("|--> get RPM for manual mode, calculated: %d", value);
    if (value > 10000) {
      return 0;
    }
  } else {
    dbg_msg("|--> get RPM using acpi");
    // getting current fan 'speed' as 'state',
    params.count = ARRAY_SIZE(args);
    params.pointer = args;
    // Args:
    // - get speed from the fan with index 'fan'
    args[0].type = ACPI_TYPE_INTEGER;
    args[0].integer.value = fan;
    dbg_msg("|--> evaluate acpi request: \\_SB.PCI0.LPCB.EC0.TACH");
    // acpi call
    ret = acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.TACH", &params,
                                &value);
    dbg_msg("|--> acpi request returned: %d", (unsigned int)ret);
    if (ret != AE_OK) {
      return -1;
    }
  }
  return (int)value;
}

// Reading the correct max fan speed does not work!
// Setting a max value has the obvious effect, thus we 'fake'
// the 'get_max' function
static int fan_get_max_speed(int fan, unsigned long *state) {
  dbg_msg("fan-id: %d | get max speed", fan);
  *state = fan_data.fan_max_speed_setting[fan];
  return 0;
}

static int fan_set_max_speed(int fan, unsigned long state, bool reset) {
  union acpi_object args[2];
  unsigned long long value;
  acpi_status ret;
  int arg_qmod = 1;

  dbg_msg("fan-id: %d | set max speed: %d, force reset: %d", fan, state,
          (unsigned int)reset);
  // if reset is 'true' ignore anything else and reset to
  // -> auto-mode with max-speed
  // -> use "SB.ARKD.QMOD" _without_ "SB.QFAN", which seems not writeable as
  // expected
  if (reset) {
    state = 255;
    arg_qmod = 2;
    // Activate the set maximum speed setting
    // Args:
    // 0 - just returns
    // 1 - sets quiet mode to QFAN value
    // 2 - sets quiet mode to 0xFF (that's the default value)
    params.count = 1;
    params.pointer = args;
    // pass arg
    args[0].type = ACPI_TYPE_INTEGER;
    args[0].integer.value = arg_qmod;
    // acpi call
    ret = acpi_evaluate_integer(NULL, "\\_SB.ATKD.QMOD", &params, &value);
    if (ret != AE_OK) {
      err_msg("set_max_speed",
              "set max fan speed(s) failed (force reset)! errcode: %d", ret);
      return ret;
    }
  } else {
    // if reset was not forced, set max fan speed to 'state'
    // is applied automatically on any available fan
    // - docs say it should affect manual _AND_ automatic mode
    // Args:
    // - from 0x00 to 0xFF (0 - 255)
    // acpi call
    params.count = ARRAY_SIZE(args);
    params.pointer = args;
    // pass arg
    args[0].type = ACPI_TYPE_INTEGER;
    args[0].integer.value = 0x521;
    args[1].type = ACPI_TYPE_INTEGER;
    args[1].integer.value = 0xc5;
    ret = acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.ST84", &params,
                                &value);
    if (ret != AE_OK) {
      err_msg("set_max_speed",
              "set max fan speed(s) failed (no reset) errcoded", ret);
      return ret;
    }
  }
  // keep set max fan speed for the get_max
  fan_data.max_fan_max_speed_setting[fan] = state;
  return ret;
}

static int fan_set_auto() {
  union acpi_object args[2];
  unsigned long long value;
  acpi_status ret;

  dbg_msg("fan-id: (both) | set to automatic mode");
  // setting (both) to auto-mode simultanously
  fan_data.fan_manual_mode[0] = false;
  fan_data.fan_state[0] = -1;
  if (fan_data.has_fan[1]) {
    fan_data.fan_manual_mode[1] = false;
    fan_data.fan_state[1] = -1;
  }
  // acpi call to call auto-mode for all fans!
  params.count = ARRAY_SIZE(args);
  params.pointer = args;
  args[0].type = ACPI_TYPE_INTEGER;
  args[0].integer.value = 0x521;
  args[1].type = ACPI_TYPE_INTEGER;
  args[1].integer.value = 0x85;
  // acpi call
  ret =
      acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.WRAM", &params, &value);
  if (ret != AE_OK) {
    err_msg("set_auto",
            "failed reseting fan(s) to auto-mode! errcode: %d - DANGER! "
            "OVERHEAT? DANGER!",
            ret);
  }
  return ret;
}

static int temp_get(int sensor) {
  acpi_status ret;
  unsigned long long int temp;

  // it seems that TH0R is the first sensor in UX410 an related laptops
  // TH1R is only used if a second / GFX fan is installed. THus we
  // read from TH0R for temperature #1 and TH1R for temperature #2
  dbg_msg("temp-id: %d | get (acpi eval)", sensor);
  // acpi call
  if (sensor == 1) {
    ret = acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.TH0R", NULL, &temp);
  } else {
    ret = acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.TH1R", NULL, &temp);
  }
  if (ret != AE_OK) {
    err_msg("temp_get", "failed reading temperature, errcode: %d", ret);
    return -1;
  }
  return int(temp);
}

#define TEMP_MIN 40
#define SLEEP_INTERVAL_MS 1000
#define TURN_OFF_AFTER_MS SLEEP_INTERVAL_MS * 3

static int control_func(void *data) {
  acpi_status result;
  union acpi_object args[2];
  unsigned long long value = 0;
  unsigned long long temp = 0;
  unsigned long long speed = 0;
  unsigned int tempBelowMinMs = 0;
  unsigned long long defaultFanSpeed0 = 0;
  params.count = ARRAY_SIZE(args);
  params.pointer = args;
  args[0].type = ACPI_TYPE_INTEGER;
  args[0].integer.value = 0;
  args[1].type = ACPI_TYPE_INTEGER;
  args[1].integer.value = 0;
  // get initial value of first entry in fan speed table
  params.count = 1;
  args[0].integer.value = 0x548;
  result = acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.RRAM", &params,
                                 &defaultFanSpeed0);
  if (result == AE_OK && defaultFanSpeed0 > 0) {
    info_msg("init", "Default minimum fan speed value is %d", defaultFanSpeed0);
  } else {
    info_msg("init",
             "Failed to read minimum fan speed value. Defaulting to 87");
    defaultFanSpeed0 = 47;
  }
  speed = defaultFanSpeed0;
  params.count = ARRAY_SIZE(args);
  info_msg("init", "Starting control thread");
  while (!kthread_should_stop()) {
    // get temperature
    result =
        acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.TH0R", NULL, &temp);
    if (result == AE_OK) {
      if (temp <= TEMP_MIN) {
        if (tempBelowMinMs >= TURN_OFF_AFTER_MS) {
          // temperature has been low for some time. set minimum fan speed in
          // fan speed table to 0
          // info_msg("run", "Temperature %d°C, turning fan off", temp);
          speed = 0;
        } else {
          tempBelowMinMs += SLEEP_INTERVAL_MS;
        }
      } else {
        // temperature too high. reset minimum fan speed in fan speed table to
        // default
        // info_msg("run", "Temperature %d°C, turning fan on", temp);
        speed = defaultFanSpeed0;
        tempBelowMinMs = 0;
      }
      args[0].integer.value = 0x548;
      args[1].integer.value = speed;
      result = acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.WRAM", &params,
                                     &value);
    }
    // check if one of the previous calls went wrong
    if (result != AE_OK) {
      // failsafe. getting temperature failed. switch to auto mode to be safe
      info_msg("run",
               "Temperature above limit or ACPI call failed. Setting fan to "
               "auto mode");
      args[0].integer.value = 0x521;
      args[1].integer.value = 0x85;
      acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.WRAM", &params, &value);
      tempBelowMinMs = 0;
    }
    msleep_interruptible(SLEEP_INTERVAL_MS);
  }
  return 0;
}

static void initialize() {
  int rpm = -1;
  int temp = -1;
  int i = 0;

  // get rpm for fans
  for (i = 0; i < 2; i++) {
    rpm = machine_default->fan_rpm(0);
    machine_default->fan_data.has_fan[i] = rpm > -1;
    if (machine_default->fan_data.has_fan[i]) {
      info_msg("init", "Machine has %s fan",
               machine_default->fan_data.fan_desc[i]);
    } else {
      err_msg("init", "Machine %s fan not found",
              machine_default->fan_data.fan_desc[i]);
    }
  }
  // get temperatures
  for (i = 0; i < 2; i++) {
    temp = machine_control->temp_get(i);
    machine_default->fan_data.has_temp[i] = temp > -1;
    if (machine_default->fan_data.has_temp[i]) {
      info_msg("init", "Machine has %s temperature sensor",
               machine_default->fan_data.fan_desc[i]);
    } else {
      err_msg("init", "Machine %s temperature sensor not found",
              machine_default->fan_data.fan_desc[i]);
    }
  }
}
