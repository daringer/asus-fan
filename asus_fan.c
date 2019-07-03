/**
 *  ASUS Fan control module
 *
 *  PLEASE USE WITH CAUTION, you can easily overheat your machine with a wrong
 *  manually set fan speed...
 *
 **/
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/dmi.h>

#include "machines.h"

MODULE_AUTHOR("Felipe Contreras <felipe.contreras@gmail.com>");
MODULE_AUTHOR("Markus Meissner <coder@safemailbox.de>");
MODULE_AUTHOR("Bernd Kast <kastbernd@gmx.de>");
MODULE_AUTHOR("Bim Overbohm <bim.overbohm@googlemail.com>");
MODULE_DESCRIPTION("ASUS fan driver (ACPI)");
MODULE_LICENSE("GPL");

//////
////// DEFINES / MACROS
//////

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

//////
////// GLOBALS
//////

const static char *fan_mode_manual_string = "manual";
const static char *fan_mode_auto_string = "auto";

// machine control and fan data interface
static struct asus_machine_control* machine_control = NULL;
// machine control thread task structure
static struct task_struct *control_task = NULL;

// params struct used frequently for acpi-call-construction
static struct acpi_object_list params;

// force loading i.e., skip device existance check and force loading of a
// specific model
static char *force_machine = "";

// housekeeping structs
static struct asus_fan_driver asus_fan_driver = {
    .name = DRIVER_NAME,
    .owner = THIS_MODULE,
};
bool driver_in_use = false;

static struct attribute *platform_attributes[] = {NULL};
static struct attribute_group platform_attribute_group = {
    .attrs = platform_attributes};

//////
////// MODULE PARAMETER(S)
//////

module_param(force_machine, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(force_machine,
                 "Force loading of module with specific machine model "
                 "identifer (currently \"Default\" and \"UX410UAK\")");

//////
////// MODULE FUNCTION PROTOTYPES
//////

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
static int asus_fan_remove(struct platform_device *device);

// prepare platform device and let it create
int __init_or_module asus_fan_register_driver(struct asus_fan_driver *driver);

// remove the driver
void asus_fan_unregister_driver(struct asus_fan_driver *driver);

// housekeeping (module) stuff...
static void __exit fan_exit(void);
static int __init fan_init(void);

//////
////// HWMON FUNCTION PROTOTYPES
//////

// regular fan api funcs
static ssize_t fan1_get_cur_state(struct device *dev,
                                 struct device_attribute *attr, char *buf);
static ssize_t fan1_set_cur_state(struct device *dev,
                                 struct device_attribute *attr, const char *buf,
                                 size_t count);
static ssize_t fan1_get_cur_control_state(struct device *dev,
                                         struct device_attribute *attr,
                                         char *buf);
static ssize_t fan1_set_cur_control_state(struct device *dev,
                                         struct device_attribute *attr,
                                         const char *buf, size_t count);
static ssize_t fan1_get_mode(struct device *dev, struct device_attribute *attr,
                             char *buf);
static ssize_t fan1_set_mode(struct device *dev, struct device_attribute *attr,
                             const char *buf, size_t count);
static ssize_t fan1_get_rpm(struct device *dev, struct device_attribute *attr,
                       char *buf);
static ssize_t fan1_get_label(struct device *dev, struct device_attribute *attr,
                         char *buf);
// writes minimal speed for auto and manual mode to buf
static ssize_t fan1_min(struct device *dev, struct device_attribute *attr,
                       char *buf);
// writes maximal speed for auto and manual mode to buf
static ssize_t fan1_set_max_speed(struct device *dev, struct device_attribute *attr,
                             const char *buf, size_t count);
// reads maximal speed for auto and manual mode to buf
static ssize_t fan1_get_max_speed(struct device *dev, struct device_attribute *attr,
                             char *buf);

static ssize_t _fan_set_mode(int fan, const char *buf, size_t count);

// read temperature value
static ssize_t temp1_input(struct device *dev, struct device_attribute *attr,
                           char *buf);
// read critical temperature value
static ssize_t temp1_crit(struct device *dev, struct device_attribute *attr,
                          char *buf);
// read temperature label
static ssize_t temp1_label(struct device *dev, struct device_attribute *attr,
                           char *buf);

// read temperature value
static ssize_t temp2_input(struct device *dev, struct device_attribute *attr,
                           char *buf);
// read critical temperature value
static ssize_t temp2_crit(struct device *dev, struct device_attribute *attr,
                          char *buf);
// read temperature label
static ssize_t temp2_label(struct device *dev, struct device_attribute *attr,
                           char *buf);

//////
////// IMPLEMENTATIONS
//////

static ssize_t fan1_rpm(struct device *dev, struct device_attribute *attr,
                       char *buf) {
  return sprintf(buf, "%d\n", machine_control->fan_rpm(0));
}
static ssize_t fan2_rpm(struct device *dev, struct device_attribute *attr,
                           char *buf) {
  return sprintf(buf, "%d\n", machine_control->fan_rpm(1));
}

/*/static ssize_t fan1_get_mode(struct device *dev, struct device_attribute *attr,
                             char *buf) {
  // = false;
  //unsigned long state = 0;
  //__fan_get_cur_state(0, &state);
  if (asus_data.fan_manual_mode[0])
    return sprintf(buf, "%s\n", fan_mode_manual_string);
  else
    return sprintf(buf, "%s\n", fan_mode_auto_string);
}
static ssize_t fan2_get_mode(struct device *dev, struct device_attribute *attr,
                             char *buf) {
  if (asus_data.fan_manual_mode[1])
    return sprintf(buf, "%s\n", fan_mode_manual_string);
  else
    return sprintf(buf, "%s\n", fan_mode_auto_string);
}*/

static ssize_t fan1_set_mode(struct device *dev, struct device_attribute *attr,
                             const char *buf, size_t count) {
  return _fan_set_mode(0, buf, count);
}

static ssize_t fan2_set_mode(struct device *dev, struct device_attribute *attr,
                             const char *buf, size_t count) {
  return _fan_set_mode(1, buf, count);
}

static ssize_t _fan_set_mode(int fan, const char *buf, size_t count) {
  int state;
  kstrtouint(buf, 10, &state);

  if (strncmp(buf, fan_mode_auto_string, strlen(fan_mode_auto_string)) == 0 ||
      strncmp(buf, "0", 1) == 0) {
    fan_set_auto();
  } else if (strncmp(buf, fan_mode_manual_string,
                     strlen(fan_mode_manual_string)) == 0)
    __fan_set_cur_state(0, (255 - asus_data.fan_minimum) >> 1);
  else
    err_msg("set mode",
            "fan id: %d | setting mode to '%s', use 'auto' or 'manual'",
            fan + 1, buf);

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
  machine_control->fan_set_cur_control_state(0, state);
  return count;
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
  machine_control->fan_set_max_speed(state, reset);
  return count;
}

static ssize_t get_max_speed(struct device *dev, struct device_attribute *attr,
                             char *buf) {
  unsigned long state = 0;
  machine_control->fan_get_max_speed(&state);
  return sprintf(buf, "%lu\n", state);
}

static ssize_t temp1_input(struct device *dev, struct device_attribute *attr,
                           char *buf) {
  acpi_status ret;
  unsigned long long int value = 0;
  ssize_t size = 0;

  machine_control->temp_get(0, &value);
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
/*static DEVICE_ATTR(pwm1, S_IWUSR | S_IRUGO, fan_get_cur_state,
                   fan_set_cur_state);
static DEVICE_ATTR(pwm1_enable, S_IWUSR | S_IRUGO, fan_get_cur_control_state,
                   fan_set_cur_control_state);

static DEVICE_ATTR(fan1_mode, S_IWUSR | S_IRUGO, fan1_get_mode, fan1_set_mode);
static DEVICE_ATTR(fan1_speed, S_IWUSR | S_IRUGO, fan_get_cur_state,
                   fan_set_cur_state);

static DEVICE_ATTR(fan1_min, S_IRUGO, fan_min, NULL);
static DEVICE_ATTR(fan1_input, S_IRUGO, fan_rpm, NULL);
static DEVICE_ATTR(fan1_label, S_IRUGO, fan_label, NULL);

static DEVICE_ATTR(fan1_max, S_IWUSR | S_IRUGO, get_max_speed, set_max_speed);

// @TODO: remove??? they are obsolete...
static DEVICE_ATTR(pwm2, S_IWUSR | S_IRUGO, fan_get_cur_state_gfx,
                   fan_set_cur_state_gfx);
static DEVICE_ATTR(pwm2_enable, S_IWUSR | S_IRUGO,
                   fan_get_cur_control_state_gfx,
                   fan_set_cur_control_state_gfx);

static DEVICE_ATTR(fan2_mode, S_IWUSR | S_IRUGO, fan2_get_mode, fan2_set_mode);
static DEVICE_ATTR(fan2_speed, S_IWUSR | S_IRUGO, fan_get_cur_state_gfx,
                   fan_set_cur_state_gfx);

static DEVICE_ATTR(fan2_min, S_IRUGO, fan_min_gfx, NULL);
static DEVICE_ATTR(fan2_max, S_IWUSR | S_IRUGO, get_max_speed, set_max_speed);
static DEVICE_ATTR(fan2_input, S_IRUGO, fan_rpm_gfx, NULL);
static DEVICE_ATTR(fan2_label, S_IRUGO, fan_label_gfx, NULL);

static DEVICE_ATTR(temp1_input, S_IRUGO, temp1_input, NULL);
static DEVICE_ATTR(temp1_label, S_IRUGO, temp1_label, NULL);
static DEVICE_ATTR(temp1_crit, S_IRUGO, temp1_crit, NULL);*/

static struct attribute *hwmon_attrs[] = {NULL, NULL, NULL, NULL, NULL, NULL,
                                          NULL, NULL, NULL, NULL, NULL, NULL,
                                          NULL, NULL, NULL, NULL, NULL, NULL,
                                          NULL, NULL, NULL, NULL, NULL};

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

static umode_t asus_hwmon_sysfs_is_visible(struct kobject *kobj,
                                           struct attribute *attr, int idx) {
  return attr->mode;
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
  if (!asus) {
    return -ENOMEM;
  }
  asus->driver = wdrv;
  asus->hwmon_dev = NULL;
  asus->platform_device = pdev;
  // set asus-dev as member into global data struct
  asus_data.asus_fan_obj = asus;
  wdrv->platform_device = pdev;
  platform_set_drvdata(asus->platform_device, asus);
  sysfs_create_group(&asus->platform_device->dev.kobj,
                     &platform_attribute_group);
  // initialize hardware monitors
  err = asus_fan_hwmon_init(asus);
  if (err) {
    asus_fan_sysfs_exit(asus->platform_device);
    kfree(asus);
    return err;
  }
  return 0;
}

static int asus_fan_remove(struct platform_device *device) {
  struct asus_fan *asus;

  dbg_msg("remove asus_fan");
  asus = platform_get_drvdata(device);
  asus_fan_sysfs_exit(asus->platform_device);
  kfree(asus);
  return 0;
}

int __init_or_module asus_fan_register_driver(struct asus_fan_driver *driver) {
  struct platform_driver *platform_driver;
  struct platform_device *platform_device;
  unsigned long state = 0;

  dbg_msg("register asus fan driver");
  if (driver_in_use) {
    return -EBUSY;
  }
  // set up driver
  platform_driver = &driver->platform_driver;
  platform_driver->remove = asus_fan_remove;
  platform_driver->driver.owner = driver->owner;
  platform_driver->driver.name = driver->name;
  // create platform device
  platform_device =
      platform_create_bundle(platform_driver, asus_fan_probe, NULL, 0, NULL, 0);
  if (IS_ERR(platform_device)) {
    return PTR_ERR(platform_device);
  }
  driver_in_use = true;
  return 0;
}

static int __init fan_init(void) {
  acpi_status ret;
  int i = 0;

  dbg_msg("starting initialization...");
  info_msg("init", "dmi sys info: '%s'", dmi_get_system_info(DMI_SYS_VENDOR));
  info_msg("init", "dmi product: '%s'", dmi_get_system_info(DMI_PRODUCT_NAME));
  dbg_msg("dmi chassis type: '%s'", dmi_get_system_info(DMI_CHASSIS_TYPE));
  // load with/without identification
  if (force_machine != NULL && strlen(force_machine) > 0) {
    info_msg("init", "forced loading of module with machine \"%s\": USE WITH CARE",
             force_machine);
  }
  machine_control =
      find_machine(dmi_get_system_info(DMI_SYS_VENDOR),
                   dmi_get_system_info(DMI_PRODUCT_NAME), force_machine);
  if (machine_control == NULL) {
    err_msg("init", "Failed to find matching machine model");
    return -ENODEV;
  }
  // we've found or forced a matching machine, initialize it
  info_msg("init", "Using machine model \"%s\"", machine_control->machine_id);
  machine_control->initialize();
  // check if reseting fan speeds works
  for (i = 0; i < 2; i++) {
    ret = machine_control->fan_set_max_speed(
        i, machine_control->fan_data.fan_max_speed_default[i], false);
    if (ret != AE_OK) {
      err_msg("init", "set max speed of fan %d to: '%d' failed! errcode: %d",
              i + 1, machine_control->fan_data.fan_max_speed_default[i], ret);
      return -ENODEV;
    }
  }
  dbg_msg("setting max fan speed succeeded, ret: %d", (unsigned int)ret);
  // force sane enviroment / init with automatic fan controlling
  if ((ret = machine_control->fan_set_auto()) != AE_OK) {
    err_msg("init", "set auto-mode speed to active, failed! errcode: %d", ret);
    return -ENODEV;
  }
  dbg_msg("setting fans to auto mode succeeded, ret: %d", (unsigned int)ret);
  // set up driver
  ret = asus_fan_register_driver(&asus_fan_driver);
  if (ret != 0) {
    err_msg("init", "registering driver failed! errcode: %d", ret);
    return ret;
  }
  // set up machine fans
  if (machine_control->fan_data.has_fan[0]) {
    hwmon_attrs[0] = &dev_attr_pwm1.attr;
    hwmon_attrs[1] = &dev_attr_pwm1_enable.attr;
    hwmon_attrs[2] = &dev_attr_fan1_min.attr;
    hwmon_attrs[3] = &dev_attr_fan1_input.attr;
    hwmon_attrs[4] = &dev_attr_fan1_label.attr;  // label
    hwmon_attrs[5] = &dev_attr_fan1_max.attr;
    hwmon_attrs[6] = &dev_attr_fan1_mode.attr;
    hwmon_attrs[7] = &dev_attr_fan1_speed.attr;
  }
  if (machine_control->fan_data.has_fan[1]) {
    hwmon_attrs[8] = &dev_attr_pwm2.attr;
    hwmon_attrs[9] = &dev_attr_pwm2_enable.attr;
    hwmon_attrs[10] = &dev_attr_fan2_min.attr;
    hwmon_attrs[11] = &dev_attr_fan2_input.attr;
    hwmon_attrs[12] = &dev_attr_fan2_label.attr;  // label
    hwmon_attrs[13] = &dev_attr_fan2_mode.attr;
    hwmon_attrs[14] = &dev_attr_fan2_speed.attr;
    hwmon_attrs[15] = &dev_attr_fan2_max.attr;
  }
  // set up temperature sensors
  if (machine_default->fan_data.has_temp[0]) {
    hwmon_attrs[16] = &dev_attr_temp1_input.attr;
    hwmon_attrs[17] = &dev_attr_temp1_label.attr;
    hwmon_attrs[18] = &dev_attr_temp1_crit.attr;
  }
  if (machine_default->fan_data.has_temp[1]) {
    hwmon_attrs[19] = &dev_attr_temp1_input.attr;
    hwmon_attrs[20] = &dev_attr_temp1_label.attr;
    hwmon_attrs[21] = &dev_attr_temp1_crit.attr;
  }
  info_msg("init", "created hwmon device: %s",
           dev_name(asus_data.asus_fan_obj->hwmon_dev));
  info_msg(
      "init", "finished init, found %d fan(s) to control",
      (unsigned int)asus_data.has_fan[0] + (unsigned int)asus_data.has_fan[1]);
  // start control thread if exists
  if (machine_control->control_thread) {
    control_task =
        kthread_run(machine_control->control_thread, NULL, "asus_fan");
  }
  return 0;
}

void asus_fan_unregister_driver(struct asus_fan_driver *driver) {
  platform_device_unregister(driver->platform_device);
  platform_driver_unregister(&driver->platform_driver);
  driver_in_use = false;
}

static void __exit fan_exit(void) {
  if (control_task) {
    kthread_stop(control_task);
    control_task = NULL;
  }
  if (machine_control) {
    machine_control->fan_set_auto();
    machine_control = NULL;
  }
  asus_fan_unregister_driver(&asus_fan_driver);
  driver_in_use = false;
  info_msg("exit", "module unloaded. cleaning up");
}

module_init(fan_init);
module_exit(fan_exit);
