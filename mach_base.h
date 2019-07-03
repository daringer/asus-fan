#ifndef __MACHINE_H_INCLUDED__

#include "driver.h"

// Data for one machine, all its fans and properties
struct asus_fan_data
{
  struct asus_fan *asus_fan_obj;
  bool has_fan[2]; // 'true' - if fan is available
  int fan_state[2]; // 'fan_states' save last (manually) set fan state/speed
  bool fan_manual_mode[2]; // 'fan_manual_mode' keeps whether this fan is manually controlled
  int fan_max_speed_default[2]; // max fan speed default
  int fan_max_speed_setting[2]; // max user-defined fan speed
  int fan_min_speed_setting[2];
  const char *fan_desc[2]; // fan name
  bool has_temp[2]; // 'true' - if temperature sensor is available
};

// List of functions to control the machine and its fans
// Use this struct to check if the machine matches and to control it
// Use the machines machine_ABC_interface() function to get this structure.
struct asus_machine_control
{
  bool (*is_machine)(const char * system_vendor, const char * product_name); // Returns true if DMI system vendor and product name match the machines
  void (*initialize)(); // Initializes the machine, checks if fans found etc. Must fill the fan_data structure
  //----- !!! call initialize() first before using any of the below functions or data !!! -----
  int (*fan_get_cur_control_state)(int fan, int *state); // Get fan state for a specific fan nr.
  int (*fan_set_cur_control_state)(int fan, int state); // Set fan RPM for a specific fan nr.
  int (*fan_set_speed)(int fan, int speed); // Set fan speed for a specific fan nr.
  int (*fan_rpm)(int fan); // Get fan RPM for a specific fan nr. (-1 on failure)
  int (*fan_get_max_speed)(int fan, unsigned long *state);
  int (*fan_set_max_speed)(int fan, unsigned long state, bool reset);
  int (*fan_set_auto)(); // Reset all fans to EC-controlled automatic mode
  int (*temp_get)(int sensor); // Get temperature from sensor nr. (-1 on failure)
  int (*control_thread)(void * data); // Optional control thread to run for machine.
  const char * machine_id; // Machine identifier, e.g. "Default" or "UX410UAK"
  asus_fan_data fan_data; // Fan data structure
};

// Default / invalid fan data. Use for initialization.
static const asus_fan_data NullAsusFanData;
// Default / invalid control data. Use for initialization.
static const asus_machine_control NullAsusMachineControl;

#endif
