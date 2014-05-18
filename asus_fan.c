/**
 *  ASUS Zenbook Fan control module, verified with:
 *  - UX32VD Zenbook
 *  - ....
 *
 *  Just 'make' and copy the fan.ko file to /lib/modules/`uname -r`/...
 *  If the modules loads succesfully it will bring up a "thermal_cooling_device"
 *  like /sys/devices/virtual/thermal/cooling_deviceX/ mostly providing
 *  cur_state / max_state
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/dmi.h>

MODULE_AUTHOR("Felipe Contreras <felipe.contreras@gmail.com>");
MODULE_DESCRIPTION("ASUS fan driver");
MODULE_LICENSE("GPL");

static struct thermal_cooling_device *cdev;

static int fan_get_max_state(struct thermal_cooling_device *cdev,
		unsigned long *state)
{
	*state = 0xff;
	return 0;
}

static int fan_get_cur_state(struct thermal_cooling_device *cdev,
		unsigned long *state)
{
	struct acpi_object_list params;
	union acpi_object in_objs[1];
	unsigned long long value;
	acpi_status r;

	params.count = ARRAY_SIZE(in_objs);
	params.pointer = in_objs;
	in_objs[0].type = ACPI_TYPE_INTEGER;
	in_objs[0].integer.value = 0;

	r = acpi_evaluate_integer(NULL, "\\_TZ.RFAN", &params, &value);
	if (r != AE_OK)
		return r;

	*state = value;

	return 0;
}

static int fan_set(struct thermal_cooling_device *cdev, int fan, int speed)
{
	struct acpi_object_list params;
	union acpi_object in_objs[2];
	unsigned long long value;

	params.count = ARRAY_SIZE(in_objs);
	params.pointer = in_objs;
	in_objs[0].type = ACPI_TYPE_INTEGER;
	in_objs[0].integer.value = fan;
	in_objs[1].type = ACPI_TYPE_INTEGER;
	in_objs[1].integer.value = speed;

	return acpi_evaluate_integer(NULL, "\\_SB.PCI0.LPCB.EC0.SFNV", &params, &value);
}

static int fan_set_cur_state(struct thermal_cooling_device *cdev,
		unsigned long state)
{
	return fan_set(cdev, 1, state);
}

static int fan_set_auto(struct thermal_cooling_device *cdev)
{
	return fan_set(cdev, 0, 0);
}

static const struct thermal_cooling_device_ops fan_cooling_ops = {
	.get_max_state = fan_get_max_state,
	.get_cur_state = fan_get_cur_state,
	.set_cur_state = fan_set_cur_state,
};

static int __init fan_init(void)
{
	if (strcmp(dmi_get_system_info(DMI_SYS_VENDOR), "ASUSTeK COMPUTER INC."))
		return -ENODEV;
	cdev = thermal_cooling_device_register("Fan", NULL, &fan_cooling_ops);
	if (IS_ERR(cdev))
		return PTR_ERR(cdev);
	fan_set_auto(cdev);
	return 0;
}

static void __exit fan_exit(void)
{
	fan_set_auto(cdev);
	thermal_cooling_device_unregister(cdev);
}

module_init(fan_init);
module_exit(fan_exit);