#ifndef __DRIVER_H_INCLUDED__

#include <linux/platform_device.h>

struct asus_fan_driver
{
  const char *name;
  struct module *owner;
  int (*probe)(struct platform_device *device);
  struct platform_driver platform_driver;
  struct platform_device *platform_device;
};

struct asus_fan
{
  struct platform_device *platform_device;
  struct asus_fan_driver *driver;
  struct asus_fan_driver *driver_gfx;
  struct device *hwmon_dev;
};

#define to_platform_driver(drv) \
  (container_of((drv), struct platform_driver, driver))
#define to_asus_fan_driver(pdrv) \
  (container_of((pdrv), struct asus_fan_driver, platform_driver))

#endif
