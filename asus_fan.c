/**
 *  ASUS Zenbook Fan control module, verified with:
 *  - UX32VD Zenbook
 *  - ....
 *
 *  Just 'make' and copy the fan.ko file to /lib/modules/`uname -r`/...
 *  If the modules loads succesfully it will bring up a "thermal_cooling_device"
 *  like /sys/devices/virtual/thermal/cooling_deviceX/ mostly providing
 *  cur_state / max_state
 *
 *  PLEASE USE WITH CAUTION, you can easily overheat your machine with a wrong
 *  manually set fan speed...
 *
**/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/dmi.h>

MODULE_AUTHOR("Felipe Contreras <felipe.contreras@gmail.com>");
MODULE_AUTHOR("Markus Meissner <coder@safemailbox.de>");
MODULE_DESCRIPTION("ASUS fan driver (ACPI)");
MODULE_LICENSE("GPL");

//////
////// GLOBALS
//////
// thermal cooling device data
static struct thermal_cooling_device *__cdev;
static struct thermal_cooling_device *__cdev_gfx;

// 'fan_states' save last (manually) set fan state/speed
static int fan_states[2] = {-1, -1};
// 'fan_manual_mode' keeps whether this fan is manually controlled
static bool fan_manual_mode[2] = {false, false};

// 'true' - if current system was identified and thus a second fan is available
static bool has_gfx_fan;

// params struct used frequently for acpi-call-construction
static struct acpi_object_list params;

// max fan speed default
static int max_fan_speed_default = 255;
// ... user-defined max value
static int max_fan_speed_setting = 255;

//// fan "name" shown in "/sys/..../cooling_deviceX/type"
// regular fan name
static char *fan_desc = "Fan";
// gfx-card fan name
static char *gfx_fan_desc = "GFX Fan";

//////
////// FUNCTION PROTOTYPES
//////

// hidden fan api funcs used for both (wrap into them)
static int __fan_get_cur_state(struct thermal_cooling_device *cdev, int fan,
                               unsigned long *state);
static int __fan_set_cur_state(struct thermal_cooling_device *cdev, int fan,
                               unsigned long state);

// regular fan api funcs
static int fan_get_cur_state(struct thermal_cooling_device *cdev,
                             unsigned long *state);
static int fan_set_cur_state(struct thermal_cooling_device *cdev,
                             unsigned long state);

// gfx fan api funcs
static int fan_get_cur_state_gfx(struct thermal_cooling_device *cdev,
                                 unsigned long *state);
static int fan_set_cur_state_gfx(struct thermal_cooling_device *cdev,
                                 unsigned long state);

// generic fan func (no sense as long as auto-mode is bound to both or none of
// the fans...
// - force 'reset' of max-speed (if reset == true) and change to auto-mode
static int fan_set_max_speed(unsigned long state, bool reset);
// acpi-readout
static int fan_get_max_speed(struct thermal_cooling_device *cdev,
                             unsigned long *state);

// set fan(s) to automatic mode
static int fan_set_auto(void);

// set fan with index 'fan' to 'speed'
// - includes manual mode activation
static int fan_set_speed(int fan, int speed);

// housekeeping (module) stuff...
static void __exit fan_exit(void);
static int __init fan_init(void);

// struct keeping primary fan callbacks
static const struct thermal_cooling_device_ops fan_cooling_ops = {
    .get_max_state = fan_get_max_speed,
    .get_cur_state = fan_get_cur_state,
    .set_cur_state = fan_set_cur_state,
};

// struct keeping secondary fan (grafix) callbacks
static const struct thermal_cooling_device_ops gfx_cooling_ops = {
    .get_max_state = fan_get_max_speed,
    .get_cur_state = fan_get_cur_state_gfx,
    .set_cur_state = fan_set_cur_state_gfx,
};

//////
////// IMPLEMENTATIONS
//////
static int __fan_get_cur_state(struct thermal_cooling_device *cdev, int fan,
                               unsigned long *state) {

  // struct acpi_object_list params;
  union acpi_object args[1];
  unsigned long long value;
  acpi_status ret;

  // getting current fan 'speed' as 'state',
  params.count = ARRAY_SIZE(args);
  params.pointer = args;
  // Args:
  // - get speed from the fan with index 'fan'
  args[0].type = ACPI_TYPE_INTEGER;
  args[0].integer.value = fan;

  // fan does not report during manual speed setting - so fake it!
  if (fan_manual_mode[fan]) {
    *state = fan_states[fan];
    return 0;
  }

  // acpi call
  ret = acpi_evaluate_integer(NULL, "\\_TZ.RFAN", &params, &value);
  if(ret != AE_OK)
    return ret;

  *state = value;
  return 0;
}

static int __fan_set_cur_state(struct thermal_cooling_device *cdev, int fan,
                               unsigned long state) {

  fan_states[fan] = state;

  // setting fan to automatic, if cur_state is set to (0x0100) 256
  if (state == 256) {
    fan_manual_mode[fan] = false;
    fan_states[fan] = -1;
    return fan_set_auto();
  } else {
    fan_manual_mode[fan] = true;
    return fan_set_speed(fan, state);
  }
}

static int fan_get_cur_state(struct thermal_cooling_device *cdev,
                             unsigned long *state) {
  *state = 0;
  return __fan_get_cur_state(cdev, 0, state);
}

static int fan_get_cur_state_gfx(struct thermal_cooling_device *cdev,
                                 unsigned long *state) {
  *state = 0;
  return __fan_get_cur_state(cdev, 1, state);
}

static int fan_set_speed(int fan, int speed) {
  // struct acpi_object_list params;
  union acpi_object args[2];
  unsigned long long value;

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

static int fan_set_cur_state_gfx(struct thermal_cooling_device *cdev,
                                 unsigned long state) {
  return __fan_set_cur_state(cdev, 1, state);
}

static int fan_set_cur_state(struct thermal_cooling_device *cdev,
                             unsigned long state) {
  return __fan_set_cur_state(cdev, 0, state);
}

// Reading the correct max fan speed does not work!
// Setting a max value has the obvious effect, thus we 'fake'
// the 'get_max' function
static int fan_get_max_speed(struct thermal_cooling_device *cdev,
                             unsigned long *state) {

  *state = max_fan_speed_setting;
  return 0;
}

static int fan_set_max_speed(unsigned long state, bool reset) {
  union acpi_object args[1];
  unsigned long long value;
  acpi_status ret;
  int arg_qmod = 1;

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
    if(ret != AE_OK) {
      printk(KERN_INFO
             "asus-fan (set_max_speed) - set max fan speed(s) failed (force "
             "reset)! errcode: %d",
             ret);
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
    ret = acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.ST98", &params, &value);
    if(ret != AE_OK) {
      printk(KERN_INFO
             "asus-fan (set_max_speed) - set max fan speed(s) failed (no "
             "reset)! errcode: %d",
             ret);
      return ret;
    }
  }

  // keep set max fan speed for the get_max
  max_fan_speed_setting = state;

  return ret;
}

static int fan_set_auto() {
  union acpi_object args[2];
  unsigned long long value;
  acpi_status ret;

  // setting (both) to auto-mode simultanously
  fan_manual_mode[0] = false;
  fan_states[0] = -1;
  if (has_gfx_fan) {
    fan_states[1] = -1;
    fan_manual_mode[1] = false;
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
  ret = acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.SFNV", &params, &value);
  if(ret != AE_OK) {
    printk(KERN_INFO
           "asus-fan (set_auto) - failed reseting fan(s) to auto-mode! "
           "errcode: %d - DANGER! OVERHEAT? DANGER!",
           ret);
    return ret;
  }

  return ret;
}

static int __init fan_init(void) {
  acpi_status ret;

  // identify system/model/platform
  if (!strcmp(dmi_get_system_info(DMI_SYS_VENDOR), "ASUSTeK COMPUTER INC.")) {
    const char *name = dmi_get_system_info(DMI_PRODUCT_NAME);

    // catching all (supported) Zenbooks _without_ a dedicated gfx-card
    if (!strcmp(name, "UX31E") || !strcmp(name, "UX21") ||
        !strcmp(name, "UX301LA") || !strcmp(name, "UX21A") ||
        !strcmp(name, "UX31A") || !strcmp(name, "UX32A") ||
        !strcmp(name, "UX42VS") || !strcmp(name, "UX302LA") ||
        !strcmp(name, "N551JK")) {
      has_gfx_fan = false;

      // this branch represents the (supported) Zenbooks with a dedicated
      // gfx-card
    } else if (!strcmp(name, "UX32VD") || !strcmp(name, "UX52VS") ||
               !strcmp(name, "UX500VZ") || !strcmp(name, "NX500")) {
      printk(
          KERN_INFO
          "asus-fan (init) - found dedicated gfx-card - second fan usable!\n");
      has_gfx_fan = true;

      // product not supported by this driver...
    } else {
      printk(KERN_INFO "asus-fan (init) - product name: '%s' unknown!\n", name);
      printk(KERN_INFO "asus-fan (init) - aborting!\n");
      return -ENODEV;
    }
    // not an ASUSTeK system ...
  } else
    return -ENODEV;

  // register primary fan (as thermal device called "Fan")
  __cdev = thermal_cooling_device_register(fan_desc, NULL, &fan_cooling_ops);
  fan_states[0] = -1;
  fan_manual_mode[0] = false;

  // if second fan is available, register it as "GFX Fan"!
  if (has_gfx_fan) {
    __cdev_gfx =
        thermal_cooling_device_register(gfx_fan_desc, NULL, &gfx_cooling_ops);
    fan_states[1] = -1;
    fan_manual_mode[1] = false;
  }

  // any errors? kick or go on!
  if (IS_ERR(__cdev)) {
    printk(KERN_INFO "asus-fan (init) - initialization of 1st fan failed!");
    return PTR_ERR(__cdev);
  }
  // uh, 2nd fan, again... any errors?
  if (IS_ERR(__cdev_gfx)) {
    printk(KERN_INFO
           "asus-fan (init) - initialization of second fan (gfx) failed!");
    return PTR_ERR(__cdev_gfx);
  }

  // set max-speed back to 'default'
  ret = fan_set_max_speed(max_fan_speed_default, false);
  if (ret != AE_OK) {
    printk(KERN_INFO
           "asus-fan (init) - set max speed to: '%d' failed! errcode: %d",
           max_fan_speed_default, ret);
    return ret;
  }

  // force sane enviroment / init with automatic fan controlling
  if ((ret = fan_set_auto()) != AE_OK) {
    printk(
        KERN_INFO
        "asus-fan (init) - set auto-mode speed to active, failed! errcode: %d",
        ret);
    return ret;
  }

  printk(KERN_INFO "asus-fan (init) - finished init\n");
  return 0;
}

static void __exit fan_exit(void) {
  fan_set_auto();
  thermal_cooling_device_unregister(__cdev);
  if (has_gfx_fan)
    thermal_cooling_device_unregister(__cdev_gfx);

  printk(KERN_INFO "asus-fan (exit) - module unloaded - cleaning up...\n");
}

module_init(fan_init);
module_exit(fan_exit);
