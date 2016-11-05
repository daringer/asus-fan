#include <linux/workqueue.h> 
#include <linux/sched.h> 
#include <linux/init.h> 
#include <linux/interrupt.h> 

struct proc_dir_entry *Our_Proc_File;

#define PROC_ENTRY_FILENAME "asus_fan_ctrl"
#define MY_WORK_QUEUE_NAME "asus_fan_ctrl.c"

static int TimerIntrpt = 0;
static void intrpt_routine(void *);
static int die = 0; /* set this to 1 for shutdown */

static struct workqueue_struct *my_workqueue;
static struct work_struct Task;
static DECLARE_WORK(Task, intrpt_routine, NULL);

static void intrpt_routine(void *irrelevant)
{
 TimerIntrpt++;
 if (die == 0)
   queue_delayed_work(my_workqueue, &Task, 100);
}

ssize_t asus_fan_ctrl_worker(char *buffer,
 char **buffer_location,
 off_t offset, int buffer_length, int *eof, void *data)
{
int len; /* The number of bytes actually used */
 /*
 * It's static so it will still be in memory
 * when we leave this function
 */
 static char my_buffer[80];
 /*
 * We give all of our information in one go, so if anybody asks us
 * if we have more information the answer should always be no.
 */
 if (offset > 0)
 return 0;
 /*
 * Fill the buffer and get its length
 */
 len = sprintf(my_buffer, "Timer called %d times so far\n", TimerIntrpt);
 /*
 * Tell the function which called us where the buffer is */
 *buffer_location = my_buffer;
 /*
 * Return the length
 */
 return len;
}
/*
 * Initialize the module − register the proc file
 */
int __init init_module()
{
 /*
 * Create our /proc file
 */
 Our_Proc_File = create_proc_entry(PROC_ENTRY_FILENAME, 0644, NULL);
 if (Our_Proc_File == NULL) {
 remove_proc_entry(PROC_ENTRY_FILENAME, &proc_root);
 printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",
 PROC_ENTRY_FILENAME);
 return −ENOMEM;
 }
 Our_Proc_File−>read_proc = procfile_read;
 Our_Proc_File−>owner = THIS_MODULE;
 Our_Proc_File−>mode = S_IFREG | S_IRUGO;
 Our_Proc_File−>uid = 0;
 Our_Proc_File−>gid = 0;
 Our_Proc_File−>size = 80;
 /*
 * Put the task in the work_timer task queue, so it will be executed at
 * next timer interrupt
 */
 my_workqueue = create_workqueue(MY_WORK_QUEUE_NAME);
 queue_delayed_work(my_workqueue, &Task, 100);
 printk(KERN_INFO "/proc/%s created\n", PROC_ENTRY_FILENAME);
 return 0;
}

/*
 * Cleanup
 */
void __exit cleanup_module()
{
 /*
 * Unregister our /proc file
 */
 remove_proc_entry(PROC_ENTRY_FILENAME, &proc_root);
 printk(KERN_INFO "/proc/%s removed\n", PROC_ENTRY_FILENAME);
 die = 1; /* keep intrp_routine from queueing itself */
 cancel_delayed_work(&Task); /* no "new ones" */
 flush_workqueue(my_workqueue); /* wait till all "old ones" finished */
 destroy_workqueue(my_workqueue);
/*
 * Sleep until intrpt_routine is called one last time. This is
 * necessary, because otherwise we'll deallocate the memory holding
 * intrpt_routine and Task while work_timer still references them.
 * Notice that here we don't allow signals to interrupt us.
 *
 * Since WaitQ is now not NULL, this automatically tells the interrupt
 * routine it's time to die.
 */
}

static ssize_t get_ctrl_state(struct device *dev,
                                 struct device_attribute *attr, char *buf);
                                 
static ssize_t set_ctrl_state(struct device *dev,
                                 struct device_attribute *attr, const char *buf,
                                 size_t count);


static DEVICE_ATTR(fan_ctrl_mode, S_IWUSR | S_IRUGO, get_ctrl_state,
                   set_ctrl_state);


static ssize_t get_ctrl_state(struct device *dev,
                                 struct device_attribute *attr, char *buf);
                                 
static ssize_t set_ctrl_state(struct device *dev,
                                 struct device_attribute *attr, const char *buf,
                                 size_t count);


