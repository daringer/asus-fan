#include "machines.h"

// Available machines. Add new ones here.
extern struct asus_machine_control machine_default;
extern struct asus_machine_control machine_ux410uak;

// The list of available machines. Add new ones here.
#define NR_OF_MACHINES 2
const asus_machine_control* internal_machines[NR_OF_MACHINES] = {
    &machine_ux410uak, &machine_default};

const asus_machine_control* find_machine(const char* system_vendor,
                                         const char* product_name,
                                         const char* force_machine) {
  if (force_machine != NULL && strlen(force_machine) > 0) {
    for (int i = 0; i < NR_OF_MACHINES; ++i) {
      if (strcmp(internal_machines[i].machine_id, force_machine) == 0) {
        return internal_machines[i];
      }
    }
  }
  for (int i = 0; i < NR_OF_MACHINES; ++i) {
    if (internal_machines[i].is_machine(system_vendor, product_name)) {
      return internal_machines[i];
    }
  }
  return NULL;
}
