#ifndef __MACHINES_H_INCLUDED__

#include "mach_base.h"

// Using machines:
// - #include "machines.h"
// - Get the DMI vendor and product of your machine.
// - Try to find a machine with: 
//   const asus_machine_control* my_machine = find_machine(vendor, product);
// - If you get a non-null pointer back a machine was found and you can use it.

// Supported machine types:
//
// Machine    | Fans | Identifer | Supported
// -----------+------+-----------+-----------
// UX21E      | 1    | Default   | Yes
// UX31E      | 1    | Default   | Yes
// UX21A      | 1    | Default   | Yes
// UX31A      | 1    | Default   | Yes
// UX32A      | 1    | Default   | Yes
// UX301LA    | 1    | Default   | Yes
// UX302LA    | 1    | Default   | Yes
// N551JK     | 1    | Default   | Yes
// N56JN      | 1    | Default   | Yes
// -----------+------+----------+-----------
// UX32VD     | 2    | Default   | Yes
// UX42VS     | 2    | Default   | Yes
// UX52VS     | 2    | Default   | Yes
// U500VZ     | 2    | Default   | Yes
// NX500      | 2    | Default   | Yes
// UX32LN     | 2    | Default   | Yes
// UX303LB    | 2    | Default   | Yes
// N552VX     | 2    | Default   | Yes
// N550JV     | 2    | Default   | Yes
// -----------+------+-----------+-----------
// UX3410U    | 1    | UX410UAK  | Limited
// UX410UAK   | 1    | UX410UAK  | Limited
// UX3300UAR  | ?    | -         | No
// Zenbook 3U | ?    | -         | No

// Default  -> mach_default.c
// UX410UAK -> mach_ux410uak.c

// Returns a machine control interface if DMI system vendor and product
// name etc. match the machines or a NULL pointer if no machine was found.
// You can pass a machine id in "force_machine" to force loading of a specific
// machine regardless of machine type checks.
const asus_machine_control* find_machine(const char* system_vendor,
                                         const char* product_name,
                                         const char* force_machine = NULL);

#endif
