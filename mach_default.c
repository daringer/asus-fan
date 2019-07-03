#include "mach_base.h"
#include "msg.h"

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
                                                      NULL,
                                                      "Default",
                                                      NullAsusFanData};

// Helpers for ACPI calls
static struct acpi_object_list params;
static union acpi_object args[2];

// Machine-specific implementations

static bool is_machine(const char *system_vendor,
                       const char * /*product_name*/) {
  return strcmp(system_vendor, "ASUSTeK COMPUTER INC.") == 0;
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
  if (machine_default->fan_data.fan_manual_mode[fan]) {
    *state = machine_default->fan_data.fan_state[fan];
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

static int fan_set_cur_state(int fan, unsigned long state) {
  dbg_msg("fan-id: %d | set state: %d", fan, state);
  // catch illegal state set
  if (state > 255) {
    warn_msg("set pwm", "illegal value provided: %d ", fan, state);
    return 1;
  }
  machine_default->fan_data.fan_state[fan] = state;
  machine_default->fan_data.fan_manual_mode[fan] = true;
  return fan_set_speed(fan, state);
}

static int fan_get_cur_control_state(int fan, int *state) {
  dbg_msg("fan-id: %d | get control state", fan);
  *state = machine_default->fan_data.fan_manual_mode[fan];
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
  unsigned long long value;
  acpi_status ret;

  dbg_msg("fan-id: %d | set speed: %d", fan, speed);
  // set speed to 'speed' for given 'fan'-index
  // -> automatically switch to manual mode!
  params.count = 2;
  params.pointer = args;
  // Args:
  // fan index
  // - add '1' to index as '0' has a special meaning (auto-mode)
  args[0].type = ACPI_TYPE_INTEGER;
  args[0].integer.value = fan + 1;
  // target fan speed
  // - between 0x00 and MAX (0 - MAX)
  // - 'MAX' is usually 0xFF (255)
  // - should be getable with fan_get_max_speed()
  args[1].type = ACPI_TYPE_INTEGER;
  args[1].integer.value = speed;
  // acpi call
  ret =
      acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.SFNV", &params, &value);
  if (ret != AE_OK) {
    err_msg("fan_set_speed", "failed to write SFNV, errcode: %d", ret);
  }
  return ret;
}

static int fan_rpm(int fan) {
  unsigned long long value;
  acpi_status ret;

  dbg_msg("fan-id: %d | get RPM", fan);
  // fan does not report during manual speed setting - so fake it!
  if (machine_default->fan_data.fan_manual_mode[fan]) {
    value =
        machine_default->fan_data.fan_state[fan] * machine_default->fan_data.fan_state[fan] * 1000 / -16054 +
        machine_default->fan_data.fan_state[fan] * 32648 / 1000 - 365;
    dbg_msg("|--> get RPM for manual mode, calculated: %d", value);
    if (value > 10000) {
      return 0;
    }
  } else {
    dbg_msg("|--> get RPM using acpi");
    // getting current fan 'speed' as 'state',
    params.count = 1;
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
  *state = machine_default->fan_data.fan_max_speed_setting[fan];
  return 0;
}

static int fan_set_max_speed(int fan, unsigned long state, bool reset) {
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
    params.count = 1;
    params.pointer = args;
    // pass arg
    args[0].type = ACPI_TYPE_INTEGER;
    args[0].integer.value = state;
    // acpi call
    ret = acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.ST98", &params,
                                &value);
    if (ret != AE_OK) {
      err_msg("set_max_speed",
              "set max fan speed(s) failed (no reset) errcoded", ret);
      return ret;
    }
  }
  // keep set max fan speed for the get_max
  machine_default->fan_data.fan_max_speed_setting[fan] = state;
  return ret;
}

static int fan_set_auto() {
  union acpi_object args[2];
  unsigned long long value;
  acpi_status ret;

  dbg_msg("fan-id: (both) | set to automatic mode");
  // setting (both) to auto-mode simultanously
  machine_default->fan_data.fan_manual_mode[0] = false;
  machine_default->fan_data.fan_state[0] = -1;
  if (machine_default->fan_data.has_fan[1]) {
    machine_default->fan_data.fan_manual_mode[1] = false;
    machine_default->fan_data.fan_state[1] = -1;
  }
  // acpi call to call auto-mode for all fans!
  params.count = 2;
  params.pointer = args;
  // special fan-id == 0 must be used
  args[0].type = ACPI_TYPE_INTEGER;
  args[0].integer.value = 0;
  // speed has to be set to zero
  args[1].type = ACPI_TYPE_INTEGER;
  args[1].integer.value = 0;
  // acpi call
  ret =
      acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.SFNV", &params, &value);
  if (ret != AE_OK) {
    err_msg("set_auto",
            "failed reseting fan(s) to auto-mode! errcode: %d - DANGER! "
            "OVERHEAT? DANGER!",
            ret);
    machine_default->fan_data.fan_manual_mode[0] = true;
    machine_default->fan_data.fan_manual_mode[1] = true;
  }
  return ret;
}

static int temp_get(int sensor) {
  acpi_status ret;
  unsigned long long int temp;

  // it seems that TH1R is the first sensor in most of the laptops
  // TH0R is only used if a second / GFX fan is installed. THus we 
  // read from TH1R for temperature #1 and TH0R for temperature #2
  dbg_msg("temp-id: %d | get (acpi eval)", sensor);
  // acpi call
  if (sensor == 1) {
    ret = acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.TH1R", NULL, &temp);
  } else {
    ret = acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.TH0R", NULL, &temp);
  }
  if (ret != AE_OK) {
    err_msg("temp_get", "failed reading temperature, errcode: %d", ret);
    return -1;
  }
  return int(temp);
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
