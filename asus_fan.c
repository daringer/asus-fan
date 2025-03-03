/**
 *  ASUS Fan control module
 *
 *  PLEASE USE WITH CAUTION, you can easily overheat your machine with a wrong
 *  manually set fan speed...
 *
**/
#include <linux/device.h>
#include <linux/err.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/platform_device.h>

MODULE_AUTHOR("Felipe Contreras <felipe.contreras@gmail.com>");
MODULE_AUTHOR("Markus Meissner <coder@safemailbox.de>");
MODULE_AUTHOR("Bernd Kast <kastbernd@gmx.de>");
MODULE_DESCRIPTION("ASUS fan driver (ACPI)");
MODULE_LICENSE("GPL");

//////
////// DEFINES / MACROS
//////

#define to_platform_driver(drv) \
  (container_of((drv), struct platform_driver, driver))
#define to_asus_fan_driver(pdrv) \
  (container_of((pdrv), struct asus_fan_driver, platform_driver))

#define DRIVER_NAME "asus_fan"
#define ASUS_FAN_VERSION "#MODULE_VERSION#"

#define TEMP1_CRIT 105
#define TEMP1_LABEL "gfx_temp"

#ifndef DEBUG
#define DEBUG (false)
#else
#undef DEBUG
#define DEBUG (true)
#endif

#define dbg_msg(fmt, ...)                                              \
  do {                                                                 \
    if (DEBUG)                                                         \
      printk(KERN_INFO "asus-fan (debug) - " fmt "\n", ##__VA_ARGS__); \
  } while (0)

#define info_msg(title, fmt, ...)                                        \
  do {                                                                   \
    printk(KERN_INFO "asus-fan (" title ") - " fmt "\n", ##__VA_ARGS__); \
  } while (0)

#define err_msg(title, fmt, ...)                                        \
  do {                                                                  \
    printk(KERN_ERR "asus-fan (" title ") - " fmt "\n", ##__VA_ARGS__); \
  } while (0)

#define warn_msg(title, fmt, ...)                                           \
  do {                                                                      \
    printk(KERN_WARNING "asus-fan (" title ") - " fmt "\n", ##__VA_ARGS__); \
  } while (0)

struct asus_fan_driver {
  const char *name;
  struct module *owner;

  int (*probe)(struct platform_device *device);

  struct platform_driver platform_driver;
  struct platform_device *platform_device;
};

struct asus_fan {
  struct platform_device *platform_device;

  struct asus_fan_driver *driver;
  struct asus_fan_driver *driver_gfx;

  struct device *hwmon_dev;
};

typedef enum {
	ASUS_FAN_HW_DEFAULT  = 0,
	ASUS_FAN_HW_UX410UAK = 1

} ASUS_FAN_HW_VARIANT;

struct asus_fan_data {
  struct asus_fan *asus_fan_obj;

  // 'fan_states' save last (manually) set fan state/speed
  int fan_states[2];
  // 'fan_manual_mode' keeps whether this fan is manually controlled
  bool fan_manual_mode[2];
  // 'true' - if first fan is available
  bool has_fan;
  // 'true' - if second fan is available
  bool has_gfx_fan;
  // max fan speed default
  int max_fan_speed_default;
  // ... user-defined max value
  int max_fan_speed_setting;
  // minimum allowed (set) speed for fan(s)
  int fan_minimum;
  // this speed will be reported as the minimal speed for the fans
  int fan_minimum_gfx;
  // regular fan name
  const char *fan_desc;
  // gfx-card fan name
  const char *gfx_fan_desc;
	// zenbook hardware variant (dmi product) group
	ASUS_FAN_HW_VARIANT variant;
};

//////
////// GLOBALS
//////


static struct asus_fan_data asus_data = {
    NULL,
		{-1, -1},
		{false, false},
		false, false,
		255, 255,
    10, 10,
		"CPU Fan", "GFX Fan",
		ASUS_FAN_HW_DEFAULT
};

const static char* fan_mode_manual_string = "manual";
const static char* fan_mode_auto_string = "auto";

// params struct used frequently for acpi-call-construction
static struct acpi_object_list params;

// force loading i.e., skip device existance check
static short force_load = false;

// allow checking but override rpm check
static short force_rpm_override = false;

// housekeeping structs
static struct asus_fan_driver asus_fan_driver = {
    .name = DRIVER_NAME, .owner = THIS_MODULE,
};
bool used;

static struct attribute *platform_attributes[] = {NULL};
static struct attribute_group platform_attribute_group = {
    .attrs = platform_attributes};

//////
////// MODULE PARAMETER(S)
//////

module_param(force_load, short, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(force_load,
                 "Force loading of module---omit device existance check");
module_param(force_rpm_override, short, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(force_rpm_override,
                 "Force loading of module---still do device existance check");

//////
////// FUNCTION PROTOTYPES
//////

// hidden fan api funcs used for both (wrap into them)
static int __fan_get_cur_state(int fan, unsigned long *state);
static int __fan_set_cur_state(int fan, unsigned long state);

// get current mode (auto, manual, perhaps auto mode of module in future)
static int __fan_get_cur_control_state(int fan, int *state);
// switch between modes (auto, manual, perhaps auto mode of module in future)
static int __fan_set_cur_control_state(int fan, int state);

// regular fan api funcs
static ssize_t fan_get_cur_state(struct device *dev,
                                 struct device_attribute *attr, char *buf);
static ssize_t fan_set_cur_state(struct device *dev,
                                 struct device_attribute *attr, const char *buf,
                                 size_t count);

// gfx fan api funcs
static ssize_t fan_get_cur_state_gfx(struct device *dev,
                                     struct device_attribute *attr, char *buf);
static ssize_t fan_set_cur_state_gfx(struct device *dev,
                                     struct device_attribute *attr,
                                     const char *buf, size_t count);

// regular fan api funcs
static ssize_t fan_get_cur_control_state(struct device *dev,
                                         struct device_attribute *attr,
                                         char *buf);
static ssize_t fan_set_cur_control_state(struct device *dev,
                                         struct device_attribute *attr,
                                         const char *buf, size_t count);

// gfx fan api funcs
static ssize_t fan_get_cur_control_state_gfx(struct device *dev,
																					 struct device_attribute *attr,
																					 char *buf);
static ssize_t fan_set_cur_control_state_gfx(struct device *dev,
																					 struct device_attribute *attr,
																					 const char *buf, size_t count);

static ssize_t _fan_set_mode(int fan, const char* buf, size_t count);

	// gfx fan api funcs
static ssize_t fan1_get_mode(struct device *dev, struct device_attribute *attr,
                             char *buf);
static ssize_t fan1_set_mode(struct device *dev, struct device_attribute *attr,
                             const char *buf, size_t count);

// gfx fan api funcs
static ssize_t fan2_get_mode(struct device *dev, struct device_attribute *attr,
                             char *buf);
static ssize_t fan2_set_mode(struct device *dev, struct device_attribute *attr,
                             const char *buf, size_t count);

// generic fan func (no sense as long as auto-mode is bound to both or none of
// the fans...
// - force 'reset' of max-speed (if reset == true) and change to auto-mode
static int fan_set_max_speed(unsigned long state, bool reset);
// acpi-readout
static int fan_get_max_speed(unsigned long *state);

// set fan(s) to automatic mode
static int fan_set_auto(void);

// set fan with index 'fan' to 'speed'
// - includes manual mode activation
static int fan_set_speed(int fan, int speed);

// reports current speed of the fan (unit:RPM)
static int __fan_rpm(int fan);

// Writes RPMs of fan0 (CPU fan) to buf => needed for hwmon device
static ssize_t fan_rpm(struct device *dev, struct device_attribute *attr,
                       char *buf);

// Writes RPMs of fan1 (GPU fan) to buf => needed for hwmon device
static ssize_t fan_rpm_gfx(struct device *dev, struct device_attribute *attr,
                           char *buf);
// Writes Label of fan0 (CPU fan) to buf => needed for hwmon device
static ssize_t fan_label(struct device *dev, struct device_attribute *attr,
                         char *buf);

// Writes Label of fan1 (GPU fan) to buf => needed for hwmon device
static ssize_t fan_label_gfx(struct device *dev, struct device_attribute *attr,
                             char *buf);
// Writes Minimal speed of fan0 (CPU fan) to buf => needed for hwmon device
static ssize_t fan_min(struct device *dev, struct device_attribute *attr,
                       char *buf);

// Writes Minimal speed of fan1 (GPU fan) to buf => needed for hwmon device
static ssize_t fan_min_gfx(struct device *dev, struct device_attribute *attr,
                           char *buf);

// sets maximal speed for auto and manual mode => needed for hwmon device
static ssize_t set_max_speed(struct device *dev, struct device_attribute *attr,
                             const char *buf, size_t count);

// writes maximal speed for auto and manual mode to buf => needed for hwmon
// device
static ssize_t get_max_speed(struct device *dev, struct device_attribute *attr,
                             char *buf);

// GFX temperature
static ssize_t temp1_input(struct device *dev, struct device_attribute *attr,
                           char *buf);
// GFX label
static ssize_t temp1_crit(struct device *dev, struct device_attribute *attr,
                          char *buf);
// GFX crit
static ssize_t temp1_label(struct device *dev, struct device_attribute *attr,
                           char *buf);

// is the hwmon interface visible?
static umode_t asus_hwmon_sysfs_is_visible(struct kobject *kobj,
                                           struct attribute *attr, int idx);

// initialization of hwmon interface
static int asus_fan_hwmon_init(struct asus_fan *asus);

// remove "asus_fan" subfolder from /sys/devices/platform
static void asus_fan_sysfs_exit(struct platform_device *device);

// set up platform device and call hwmon init
static int asus_fan_probe(struct platform_device *pdev);

// do anything needed to remove platform device
static void asus_fan_remove(struct platform_device *device);

// prepare platform device and let it create
int __init_or_module asus_fan_register_driver(struct asus_fan_driver *driver);

// remove the driver
void asus_fan_unregister_driver(struct asus_fan_driver *driver);

// housekeeping (module) stuff...
static void __exit fan_exit(void);
static int __init fan_init(void);

//////
////// IMPLEMENTATIONS
//////
static int __fan_get_cur_state(int fan, unsigned long *state) {

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
  int rpm = __fan_rpm(fan);

  dbg_msg("fan-id: %d | get RPM", fan);

  if (asus_data.fan_manual_mode[fan]) {
    *state = asus_data.fan_states[fan];
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

static int __fan_set_cur_state(int fan, unsigned long state) {

  dbg_msg("fan-id: %d | set state: %d", fan, state);
  // catch illegal state set
  if (state > 255) {
    warn_msg("set pwm", "illegal value provided: %d ", fan, state);
    return 1;
  }

  asus_data.fan_states[fan] = state;
  asus_data.fan_manual_mode[fan] = true;
  return fan_set_speed(fan, state);
}

static int __fan_get_cur_control_state(int fan, int *state) {
  dbg_msg("fan-id: %d | get control state", fan);
  *state = asus_data.fan_manual_mode[fan];
  return 0;
}

static int __fan_set_cur_control_state(int fan, int state) {
  dbg_msg("fan-id: %d | set control state: %d", fan, state);
  if (state == 0) {
    return fan_set_auto();
  }
  return 0;
}
static int fan_set_speed(int fan, int speed) {
  union acpi_object args[2];
  unsigned long long value;

  dbg_msg("fan-id: %d | set speed: %d", fan, speed);

  // set speed to 'speed' for given 'fan'-index
  // -> automatically switch to manual mode!
  params.count = ARRAY_SIZE(args);
  params.pointer = args;
  // Args:
  // fan index
  // - add '1' to index as '0' has a special meaning (auto-mode)
  args[0].type = ACPI_TYPE_INTEGER;
  args[0].integer.value = fan + 1;
  // target fan speed
  // - between 0x00 and MAX (0 - MAX)
  //   - 'MAX' is usually 0xFF (255)
  //   - should be getable with fan_get_max_speed()
  args[1].type = ACPI_TYPE_INTEGER;
  args[1].integer.value = speed;
  // acpi call
  return acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.SFNV", &params,
                               &value);
}

static int __fan_rpm(int fan) {
  struct acpi_object_list params;
  union acpi_object args[1];
  unsigned long long value;
  acpi_status ret;

  dbg_msg("fan-id: %d | get RPM", fan);

  // fan does not report during manual speed setting - so fake it!
  if (asus_data.fan_manual_mode[fan]) {
    value =
        asus_data.fan_states[fan] * asus_data.fan_states[fan] * 1000 / -16054 +
        asus_data.fan_states[fan] * 32648 / 1000 - 365;

    dbg_msg("|--> get RPM for manual mode, calculated: %d", value);

    if (value > 10000)
      return 0;
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
    dbg_msg("|--> acpi request returned: %s", acpi_format_exception(ret));
    if (ret != AE_OK)
      return -1;
  }
  return (int)value;
}
static ssize_t fan_rpm(struct device *dev, struct device_attribute *attr,
                       char *buf) {
  return sprintf(buf, "%d\n", __fan_rpm(0));
}
static ssize_t fan_rpm_gfx(struct device *dev, struct device_attribute *attr,
                           char *buf) {
  return sprintf(buf, "%d\n", __fan_rpm(1));
}

static ssize_t fan1_get_mode(struct device *dev,
                                 struct device_attribute *attr, char *buf) {
  /* = false;
  unsigned long state = 0;
  __fan_get_cur_state(0, &state);*/
	if (asus_data.fan_manual_mode[0])
		return sprintf(buf, "%s\n", fan_mode_manual_string);
  else
		return sprintf(buf, "%s\n", fan_mode_auto_string);
}
static ssize_t fan2_get_mode(struct device *dev,
                                     struct device_attribute *attr, char *buf) {
	if (asus_data.fan_manual_mode[1])
		return sprintf(buf, "%s\n", fan_mode_manual_string);
  else
		return sprintf(buf, "%s\n", fan_mode_auto_string);
}

static ssize_t fan1_set_mode(struct device *dev,
                                     struct device_attribute *attr,
                                     const char *buf, size_t count) {
	return _fan_set_mode(0, buf, count);
}

static ssize_t fan2_set_mode(struct device *dev,
                                 struct device_attribute *attr, const char *buf,
                                 size_t count) {
	return _fan_set_mode(1, buf, count);
}

static ssize_t _fan_set_mode(int fan, const char* buf, size_t count) {
  int state;
  kstrtouint(buf, 10, &state);

	if (strncmp(buf, fan_mode_auto_string, strlen(fan_mode_auto_string)) == 0 || strncmp(buf, "0", 1) == 0) {
		fan_set_auto();
	} else if (strncmp(buf, fan_mode_manual_string, strlen(fan_mode_manual_string)) == 0)
		__fan_set_cur_state(0, (255 - asus_data.fan_minimum) >> 1);
	else
  	err_msg("set mode", "fan id: %d | setting mode to '%s', use 'auto' or 'manual'", fan+1, buf);

  return count;
}

// @FIXME: OBSOLETE
static ssize_t fan_get_cur_state(struct device *dev,
                                 struct device_attribute *attr, char *buf) {
  unsigned long state = 0;
  __fan_get_cur_state(0, &state);
  return sprintf(buf, "%lu\n", state);
}

// @FIXME: OBSOLETE
static ssize_t fan_get_cur_state_gfx(struct device *dev,
                                     struct device_attribute *attr, char *buf) {
  unsigned long state = 0;
  __fan_get_cur_state(1, &state);
  return sprintf(buf, "%lu\n", state);
}

// @FIXME: OBSOLETE
static ssize_t fan_set_cur_state_gfx(struct device *dev,
                                     struct device_attribute *attr,
                                     const char *buf, size_t count) {
  int state;
  kstrtouint(buf, 10, &state);
  __fan_set_cur_state(1, state);
  return count;
}

// @FIXME: OBSOLETE
static ssize_t fan_set_cur_state(struct device *dev,
                                 struct device_attribute *attr, const char *buf,
                                 size_t count) {
  int state;
  kstrtouint(buf, 10, &state);
  __fan_set_cur_state(0, state);
  return count;
}

static ssize_t fan_get_cur_control_state(struct device *dev,
                                         struct device_attribute *attr,
                                         char *buf) {
  int state = 0;
  __fan_get_cur_control_state(0, &state);
  return sprintf(buf, "%d\n", state);
}

static ssize_t fan_get_cur_control_state_gfx(struct device *dev,
                                             struct device_attribute *attr,
                                             char *buf) {
  int state = 0;
  __fan_get_cur_control_state(1, &state);
  return sprintf(buf, "%d\n", state);
}

static ssize_t fan_set_cur_control_state_gfx(struct device *dev,
                                             struct device_attribute *attr,
                                             const char *buf, size_t count) {
  int state;
  kstrtouint(buf, 10, &state);
  __fan_set_cur_control_state(1, state);
  return count;
}

static ssize_t fan_set_cur_control_state(struct device *dev,
                                         struct device_attribute *attr,
                                         const char *buf, size_t count) {
  int state;
  kstrtouint(buf, 10, &state);
  __fan_set_cur_control_state(0, state);
  return count;
}

// Reading the correct max fan speed does not work!
// Setting a max value has the obvious effect, thus we 'fake'
// the 'get_max' function
static int fan_get_max_speed(unsigned long *state) {

  dbg_msg("fan-id: (both) | get max speed");
  *state = asus_data.max_fan_speed_setting;
  return 0;
}

static int fan_set_max_speed(unsigned long state, bool reset) {
  union acpi_object args[1];
  unsigned long long value;
  acpi_status ret;
  int arg_qmod = 1;

  dbg_msg("fan-id: (both) | set max speed: %d, force reset: %d", state,
          (unsigned int)reset);

  // if reset is 'true' ignore anything else and reset to
  // -> auto-mode with max-speed
  // -> use "SB.ARKD.QMOD" _without_ "SB.QFAN",
  //    which seems not writeable as expected
  if (reset) {
    state = 255;
    arg_qmod = 2;
    // Activate the set maximum speed setting
    // Args:
    // 0 - just returns
    // 1 - sets quiet mode to QFAN value
    // 2 - sets quiet mode to 0xFF (that's the default value)
    params.count = ARRAY_SIZE(args);
    params.pointer = args;
    // pass arg
    args[0].type = ACPI_TYPE_INTEGER;
    args[0].integer.value = arg_qmod;

    // acpi call
    ret = acpi_evaluate_integer(NULL, "\\_SB.ATKD.QMOD", &params, &value);
    if (ret != AE_OK) {
      err_msg("set_max_speed",
              "set max fan speed(s) failed (force reset)! errcode: %s",
              acpi_format_exception(ret));
      return ret;
    }

    // if reset was not forced, set max fan speed to 'state'
  } else {
    // is applied automatically on any available fan
    // - docs say it should affect manual _AND_ automatic mode
    // Args:
    // - from 0x00 to 0xFF (0 - 255)
    params.count = ARRAY_SIZE(args);
    params.pointer = args;
    // pass arg
    args[0].type = ACPI_TYPE_INTEGER;
    args[0].integer.value = state;

    // acpi call
    ret = acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.ST98", &params,
                                &value);
    if (ret != AE_OK) {
      err_msg("set_max_speed",
              "set max fan speed(s) failed (no reset) errcode: %s",
              acpi_format_exception(ret));

      return ret;
    }
  }

  // keep set max fan speed for the get_max
  asus_data.max_fan_speed_setting = state;

  return ret;
}

static int fan_set_auto() {
  union acpi_object args[2];
  unsigned long long value;
  acpi_status ret;

  dbg_msg("fan-id: (both) | set to automatic mode");

  // setting (both) to auto-mode simultanously
  asus_data.fan_manual_mode[0] = false;
  asus_data.fan_states[0] = -1;
  if (asus_data.has_gfx_fan) {
    asus_data.fan_states[1] = -1;
    asus_data.fan_manual_mode[1] = false;
  }

  // acpi call to call auto-mode for all fans!
  params.count = ARRAY_SIZE(args);
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
            "failed reseting fan(s) to auto-mode! "
            "errcode: %s - DANGER! OVERHEAT? DANGER!",
            acpi_format_exception(ret));

    return ret;
  }
  return ret;
}

static ssize_t fan_label(struct device *dev, struct device_attribute *attr,
                         char *buf) {
  return sprintf(buf, "%s\n", asus_data.fan_desc);
}

static ssize_t fan_label_gfx(struct device *dev, struct device_attribute *attr,
                             char *buf) {
  return sprintf(buf, "%s\n", asus_data.gfx_fan_desc);
}
static ssize_t fan_min(struct device *dev, struct device_attribute *attr,
                       char *buf) {
  return sprintf(buf, "%d\n", asus_data.fan_minimum);
}

static ssize_t fan_min_gfx(struct device *dev, struct device_attribute *attr,
                           char *buf) {
  return sprintf(buf, "%d\n", asus_data.fan_minimum_gfx);
}

static ssize_t set_max_speed(struct device *dev, struct device_attribute *attr,
                             const char *buf, size_t count) {
  int state;
  bool reset = false;
  kstrtouint(buf, 10, &state);
  if (state == 256) {
    reset = true;
  }
  fan_set_max_speed(state, reset);
  return count;
}

static ssize_t get_max_speed(struct device *dev, struct device_attribute *attr,
                             char *buf) {
  unsigned long state = 0;
  fan_get_max_speed(&state);
  return sprintf(buf, "%lu\n", state);
}

static ssize_t temp1_input(struct device *dev, struct device_attribute *attr,
                           char *buf) {
  acpi_status ret;
  unsigned long long int value;
  ssize_t size = 0;

  dbg_msg("temp-id: 1 | get (acpi eval)");

  // acpi call
  ret = acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.TH1R", NULL, &value);
  if (ret != AE_OK) {
    err_msg("read_temp", "failed reading temperature, errcode: %s",
     acpi_format_exception(ret));
    return ret;
  }
  size = sprintf((char *)&buf, "%llu\n", value);
  return size;
}

static ssize_t temp1_label(struct device *dev, struct device_attribute *attr,
                           char *buf) {
  return sprintf(buf, "%s\n", TEMP1_LABEL);
}

static ssize_t temp1_crit(struct device *dev, struct device_attribute *attr,
                          char *buf) {
  return sprintf(buf, "%d\n", TEMP1_CRIT);
}

// Makros defining all possible hwmon attributes
// @TODO: remove??? they are obsolete...
static DEVICE_ATTR(pwm1, S_IWUSR | S_IRUGO, fan_get_cur_state,
                   fan_set_cur_state);
static DEVICE_ATTR(pwm1_enable, S_IWUSR | S_IRUGO, fan_get_cur_control_state,
                   fan_set_cur_control_state);

static DEVICE_ATTR(fan1_mode, S_IWUSR | S_IRUGO, fan1_get_mode, fan1_set_mode);
static DEVICE_ATTR(fan1_speed, S_IWUSR | S_IRUGO, fan_get_cur_state, fan_set_cur_state);

static DEVICE_ATTR(fan1_min, S_IRUGO, fan_min, NULL);
static DEVICE_ATTR(fan1_input, S_IRUGO, fan_rpm, NULL);
static DEVICE_ATTR(fan1_label, S_IRUGO, fan_label, NULL);

static DEVICE_ATTR(fan1_max, S_IWUSR | S_IRUGO, get_max_speed,
                   set_max_speed);

// @TODO: remove??? they are obsolete...
static DEVICE_ATTR(pwm2, S_IWUSR | S_IRUGO, fan_get_cur_state_gfx,
								 fan_set_cur_state_gfx);
static DEVICE_ATTR(pwm2_enable, S_IWUSR | S_IRUGO,
                   fan_get_cur_control_state_gfx,
                   fan_set_cur_control_state_gfx);

static DEVICE_ATTR(fan2_mode, S_IWUSR | S_IRUGO, fan2_get_mode, fan2_set_mode);
static DEVICE_ATTR(fan2_speed, S_IWUSR | S_IRUGO, fan_get_cur_state_gfx, fan_set_cur_state_gfx);



static DEVICE_ATTR(fan2_min, S_IRUGO, fan_min_gfx, NULL);
static DEVICE_ATTR(fan2_max, S_IWUSR | S_IRUGO, get_max_speed,
                   set_max_speed);
static DEVICE_ATTR(fan2_input, S_IRUGO, fan_rpm_gfx, NULL);
static DEVICE_ATTR(fan2_label, S_IRUGO, fan_label_gfx, NULL);

static DEVICE_ATTR(temp1_input, S_IRUGO, temp1_input, NULL);
static DEVICE_ATTR(temp1_label, S_IRUGO, temp1_label, NULL);
static DEVICE_ATTR(temp1_crit, S_IRUGO, temp1_crit, NULL);

static struct attribute *hwmon_attrs[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                                          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                                          NULL, NULL, NULL, NULL,

                                          NULL};

// by now sysfs is always visible
static umode_t asus_hwmon_sysfs_is_visible(struct kobject *kobj,
                                           struct attribute *attr, int idx) {
  return attr->mode;
}

static struct attribute_group hwmon_attr_group = {
    .is_visible = asus_hwmon_sysfs_is_visible, .attrs = hwmon_attrs};
// will create hwmon_attr_groups (?)
__ATTRIBUTE_GROUPS(hwmon_attr);

static int asus_fan_hwmon_init(struct asus_fan *asus) {

  dbg_msg("init hwmon device");

  asus->hwmon_dev = hwmon_device_register_with_groups(
      &asus->platform_device->dev, "asus_fan", asus, hwmon_attr_groups);

  if (IS_ERR(asus->hwmon_dev)) {
    err_msg("init", "could not register asus hwmon device");
    return PTR_ERR(asus->hwmon_dev);
  }

  return 0;
}

static void asus_fan_sysfs_exit(struct platform_device *device) {
  dbg_msg("remove hwmon device");
  sysfs_remove_group(&device->dev.kobj, &platform_attribute_group);
}

static int asus_fan_probe(struct platform_device *pdev) {
  struct platform_driver *pdrv = to_platform_driver(pdev->dev.driver);
  struct asus_fan_driver *wdrv = to_asus_fan_driver(pdrv);

  struct asus_fan *asus;
  int err = 0;

  dbg_msg("probe for device");

  asus = kzalloc(sizeof(struct asus_fan), GFP_KERNEL);
  if (!asus)
    return -ENOMEM;

  asus->driver = wdrv;
  asus->hwmon_dev = NULL;
  asus->platform_device = pdev;

  // set asus-dev as member into global data struct
  asus_data.asus_fan_obj = asus;

  wdrv->platform_device = pdev;
  platform_set_drvdata(asus->platform_device, asus);

  sysfs_create_group(&asus->platform_device->dev.kobj,
                     &platform_attribute_group);

  err = asus_fan_hwmon_init(asus);
  if (err)
    goto fail_hwmon;

  return 0;

fail_hwmon:
  asus_fan_sysfs_exit(asus->platform_device);
  kfree(asus);
  return err;
}

static void asus_fan_remove(struct platform_device *device) {
  struct asus_fan *asus;

  dbg_msg("remove asus_fan");

  asus = platform_get_drvdata(device);
  asus_fan_sysfs_exit(asus->platform_device);
  kfree(asus);
}

int __init_or_module asus_fan_register_driver(struct asus_fan_driver *driver) {
  struct platform_driver *platform_driver;
  struct platform_device *platform_device;
  unsigned long state = 0;

  dbg_msg("register asus fan driver");

  if (used) {
    return -EBUSY;
  }
  platform_driver = &driver->platform_driver;
  platform_driver->remove = asus_fan_remove;
  platform_driver->driver.owner = driver->owner;
  platform_driver->driver.name = driver->name;

  platform_device =
      platform_create_bundle(platform_driver, asus_fan_probe, NULL, 0, NULL, 0);
  if (IS_ERR(platform_device)) {
    return PTR_ERR(platform_device);
  }

	// check current fan(s) state (manual or auto)
  if (__fan_get_cur_state(0, &state) != 0) {
		asus_data.fan_manual_mode[0] = true;
		asus_data.fan_manual_mode[1] = true;
	}

  used = true;
  return 0;
}

static int __init fan_init(void) {
  acpi_status ret;
  int rpm;

  dbg_msg("starting initialization...");
  info_msg("init", "dmi sys info: '%s'", dmi_get_system_info(DMI_SYS_VENDOR));
  info_msg("init", "dmi product: '%s'", dmi_get_system_info(DMI_PRODUCT_NAME));
  dbg_msg("dmi chassis type: '%s'", dmi_get_system_info(DMI_CHASSIS_TYPE));

  // load without identification
  if (force_load) {
    asus_data.has_gfx_fan = true;
    info_msg("init", "forced loading of module: USE WITH CARE");

    // identify system/model/platform
  } else if (!strcmp(dmi_get_system_info(DMI_SYS_VENDOR),
                     "ASUSTeK COMPUTER INC.")) {

    // step by step probe available functionalities and insert into attrib grp
    // @TODO TODO TODO TODO

		size_t temp = AE_OK;
    // USE this for idx in hwmon_attrs size_t idx = 0;
    // try to get RPM for first fan
    rpm = __fan_rpm(0);
    if (force_rpm_override){
      info_msg("init", "overriding rpm check: USE WITH CARE");
    }
    if (rpm == -1 && !force_rpm_override) {
      asus_data.has_fan = false;
      err_msg("init", "fan-id: 1 | failed to get rpm");
    } else {
      asus_data.has_fan = true;
      info_msg("init", "fan-id: 1 | success getting rpm");
      hwmon_attrs[0] = &dev_attr_pwm1.attr;
      hwmon_attrs[1] = &dev_attr_pwm1_enable.attr;
      hwmon_attrs[2] = &dev_attr_fan1_min.attr;
      hwmon_attrs[3] = &dev_attr_fan1_input.attr;
      hwmon_attrs[5] = &dev_attr_fan1_max.attr;
      hwmon_attrs[14] = &dev_attr_fan1_mode.attr;
      hwmon_attrs[15] = &dev_attr_fan1_speed.attr;
    }
    // try to get RPM for second fan
    rpm = __fan_rpm(1);
    if (rpm == -1 && !force_rpm_override) {
      err_msg("init", "fan-id: 2 | failed to get rpm");
      asus_data.has_gfx_fan = false;
    } else {
      info_msg("init", "fan-id: 2 | success getting rpm");
      asus_data.has_gfx_fan = true;
      hwmon_attrs[6] = &dev_attr_pwm2.attr;
      hwmon_attrs[7] = &dev_attr_pwm2_enable.attr;
      hwmon_attrs[8] = &dev_attr_fan2_min.attr;
      hwmon_attrs[9] = &dev_attr_fan2_input.attr;
      hwmon_attrs[16] = &dev_attr_fan2_mode.attr;
      hwmon_attrs[17] = &dev_attr_fan2_speed.attr;
      hwmon_attrs[18] = &dev_attr_fan2_max.attr;
    }

    // try to read temprature
    // size_t temp = temp1_input(asus_data.hwmon_dev, ); // @TODO
    if (temp != AE_OK) {
      err_msg("init", "temperature read probe failed");
    } else {
      info_msg("init", "temp-id: 1 | success getting temp");
      hwmon_attrs[11] = &dev_attr_temp1_input.attr;
      hwmon_attrs[12] = &dev_attr_temp1_label.attr;
      hwmon_attrs[13] = &dev_attr_temp1_crit.attr;
    }

    // set labels for existing fans
    if (asus_data.has_fan)
      hwmon_attrs[4] = &dev_attr_fan1_label.attr;
    if (asus_data.has_gfx_fan)
      hwmon_attrs[10] = &dev_attr_fan2_label.attr;

    // check if reseting fan speeds works
    ret = fan_set_max_speed(asus_data.max_fan_speed_default, false);
    if (ret != AE_OK) {
      err_msg("init", "set max speed to: '%d' failed! errcode: %s",
              asus_data.max_fan_speed_default, acpi_format_exception(ret));
      return -ENODEV;
    }

    dbg_msg("fan_set_max_speed() call succeeded, ret: %s",
     acpi_format_exception(ret));

    // force sane enviroment / init with automatic fan controlling
    if ((ret = fan_set_auto()) != AE_OK) {
      err_msg("init", "set auto-mode speed to active, failed! errcode: %s",
              acpi_format_exception(ret));
      return -ENODEV;
    }

    dbg_msg("fan_set_auto() call succeeded, ret: %s",
     acpi_format_exception(ret));
  }

  ret = asus_fan_register_driver(&asus_fan_driver);
  if (ret != AE_OK) {
    err_msg("init", "set max speed to: '%d' failed! errcode: %s",
            asus_data.max_fan_speed_default, acpi_format_exception(ret));
    return ret;
  }

  info_msg("init", "created hwmon device: %s",
           dev_name(asus_data.asus_fan_obj->hwmon_dev));
  info_msg("init", "finished init, found %d fan(s) to control",
           (unsigned int)asus_data.has_gfx_fan + 1);
  return 0;
}

void asus_fan_unregister_driver(struct asus_fan_driver *driver) {
  platform_device_unregister(driver->platform_device);
  platform_driver_unregister(&driver->platform_driver);
  used = false;
}

static void __exit fan_exit(void) {
  fan_set_auto();
  asus_fan_unregister_driver(&asus_fan_driver);
  used = false;

  info_msg("exit", "module unloaded---cleaning up");
}

module_init(fan_init);
module_exit(fan_exit);
