#ifndef __MSG_H_INCLUDED__

#include <linux/kernel.h>

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

#endif
