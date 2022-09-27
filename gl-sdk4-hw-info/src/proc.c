#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/version.h>
#include "gl-hw-info.h"

static int proc_show(struct seq_file *s, void *v)
{
    const char *value = s->private;

    seq_printf(s, "%s\n", value);

    return 0;
}


static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_show, PDE_DATA(inode));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
struct file_operations proc_fops = {
    .owner = THIS_MODULE,
    .read = seq_read,
    .open = proc_open,
    .llseek = seq_lseek,
    .release = single_release,
};
#else
struct proc_ops proc_fops = {
    .proc_read = seq_read,
    .proc_open = proc_open,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#endif

struct proc_dir_entry *create_proc_node(const char *name, const char *value)
{
    return proc_create_data(name, 0, gl_hw_info.parent, &proc_fops, (void *)value);
}

int proc_init_gl_hw_info(void)
{
    gl_hw_info.parent = proc_mkdir(PROC_DIR, NULL);
    if (!gl_hw_info.parent)
        return -ENOENT;

    return 0;
}

int proc_remove_gl_hw_info(void)
{
    proc_remove(gl_hw_info.parent);
    return 0;
}
