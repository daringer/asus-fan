#include "mach_base.h"

const asus_fan_data NullAsusFanData =
{
  NULL,
  {false, false},
  {-1, -1},
  {false, false},
  {255, 255},
  {255, 255},
  {10, 10},
  {"CPU", "GFX"},
  {false, false}
};

const asus_machine_control NullAsusMachineControl =
{
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  "",
  NULL,
  NullAsusFanData
};