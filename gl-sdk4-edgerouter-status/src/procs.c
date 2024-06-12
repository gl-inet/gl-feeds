#include <linux/proc_fs.h>
#include <linux/inet.h>
#include <linux/version.h>

//#include <time.h>
#include "erdevices.h"

static int strtomac(const char *str, u8 mac[ETH_ALEN])
{
    if (sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &mac[0],
               &mac[1],
               &mac[2],
               &mac[3],
               &mac[4],
               &mac[5]) == 6) {
        return 0;
    } else {
        return -1;
    }
}

static void format_str(char *str)
{
    if (str[strlen(str) - 1] == '\n')
        str[strlen(str) - 1] = '\0';
}

static int erdevices_show(struct seq_file *s, void *v)
{

    seq_printf(s, "MAC\tIP\tLastalive\tARPalive\n");
    show_erdevice(s);
    return 0;
}

static ssize_t erdevices_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    char conf[128] = "";
    u8 mac[ETH_ALEN];

    if (size > sizeof(conf) - 1)
        return -EINVAL;

    memset(conf, 0, sizeof(conf));
    if (copy_from_user(conf, buf, size))
        return -EFAULT;
    format_str(conf);
    if (strcmp("flush", conf) == 0) {
        flush_devices();
        return size;
    }

    if (0 != strtomac(conf, mac))
        return -EINVAL;

    new_erdevices(mac);

    return size;
}

static int erdevices_open(struct inode *inode, struct file *file)
{
    return single_open(file, erdevices_show, NULL);
}

static int target_ip_show(struct seq_file *s, void *v)
{

    seq_printf(s, "%pI4/%d\n", (void *)&erdevice_target_ip, inet_mask_len(erdevice_target_mask));
    return 0;
}

static ssize_t target_ip_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    char conf[128] = "";
    char *mask, *endp;
    __be32 ip, m = 0;

    if (size > sizeof(conf) - 1)
        return -EINVAL;
    memset(conf, 0, sizeof(conf));
    if (copy_from_user(conf, buf, size))
        return -EFAULT;
    format_str(conf);

    mask = strchr(conf, '/');
    if (mask)
        *mask++ = 0;


    if (!in4_pton(conf, strlen(conf), (u8 *)&ip, '\0', NULL)) {
        return -EINVAL;
    }

    if (mask) {
        m = simple_strtoul(mask, &endp, 10);
        if (endp == mask || m > 32)
            return -EINVAL;
    }

    erdevice_target_ip = ip;
    erdevice_target_mask = inet_make_mask(m);
    return size;
}

static int target_ip_open(struct inode *inode, struct file *file)
{
    return single_open(file, target_ip_show, NULL);
}


static int target_mac_show(struct seq_file *s, void *v)
{

    seq_printf(s, "%pM\n", erdevice_target_mac);
    return 0;
}

static ssize_t target_mac_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    char conf[128] = "";
    u8 mac[ETH_ALEN];

    if (size > sizeof(conf) - 1)
        return -EINVAL;

    memset(conf, 0, sizeof(conf));
    if (copy_from_user(conf, buf, size))
        return -EFAULT;
    format_str(conf);

    if (0 != strtomac(conf, mac))
        return -EINVAL;

    memcpy(erdevice_target_mac, mac, ETH_ALEN);
    return size;
}

static int target_mac_open(struct inode *inode, struct file *file)
{
    return single_open(file, target_mac_show, NULL);
}

static int mode_show(struct seq_file *s, void *v)
{

    seq_printf(s, "%d\n", erdevice_work_mode);
    return 0;
}

static ssize_t mode_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    int ret;
    int res;
    ret = kstrtoint_from_user(buf, size, 10, &res);
    if (ret) {
        return ret;
    }
    erdevice_work_mode = res;
    return size;
}

static int mode_open(struct inode *inode, struct file *file)
{
    return single_open(file, mode_show, NULL);
}

static int iface_show(struct seq_file *s, void *v)
{

    seq_printf(s, "%s\n", erdevice_iface);
    return 0;
}

static ssize_t iface_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    char conf[128] = "";

    if (size > sizeof(conf) - 1)
        return -EINVAL;
    memset(conf, 0, sizeof(conf));
    if (copy_from_user(conf, buf, size))
        return -EFAULT;
    format_str(conf);

    if (size > 0 && size < IFNAMSIZ) {
        memcpy(erdevice_iface, conf, size);
    } else {
        return -EFAULT;
    }

    return size;
}

static int iface_open(struct inode *inode, struct file *file)
{
    return single_open(file, iface_show, NULL);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
const static struct file_operations erdevices_ops = {
    .owner      = THIS_MODULE,
    .open       = erdevices_open,
    .read       = seq_read,
    .write      = erdevices_write,
    .llseek     = seq_lseek,
    .release    = single_release
};
#else
const static struct proc_ops erdevices_ops = {
    .proc_open       = erdevices_open,
    .proc_read       = seq_read,
    .proc_write      = erdevices_write,
    .proc_lseek     = seq_lseek,
    .proc_release    = single_release
};
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
const static struct file_operations target_ip_ops = {
    .owner      = THIS_MODULE,
    .open       = target_ip_open,
    .read       = seq_read,
    .write      = target_ip_write,
    .llseek     = seq_lseek,
    .release    = single_release
};
#else
const static struct proc_ops target_ip_ops = {
    .proc_open       = target_ip_open,
    .proc_read       = seq_read,
    .proc_write      = target_ip_write,
    .proc_lseek     = seq_lseek,
    .proc_release    = single_release
};
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
const static struct file_operations target_mac_ops = {
    .owner      = THIS_MODULE,
    .open       = target_mac_open,
    .read       = seq_read,
    .write      = target_mac_write,
    .llseek     = seq_lseek,
    .release    = single_release
};
#else
const static struct proc_ops target_mac_ops = {
    .proc_open       = target_mac_open,
    .proc_read       = seq_read,
    .proc_write      = target_mac_write,
    .proc_lseek     = seq_lseek,
    .proc_release    = single_release
};
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
const static struct file_operations mode_ops = {
    .owner      = THIS_MODULE,
    .open       = mode_open,
    .read       = seq_read,
    .write      = mode_write,
    .llseek     = seq_lseek,
    .release    = single_release
};
#else
const static struct proc_ops mode_ops = {
    .proc_open       = mode_open,
    .proc_read       = seq_read,
    .proc_write      = mode_write,
    .proc_lseek     = seq_lseek,
    .proc_release    = single_release
};
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
const static struct file_operations iface_ops = {
    .owner      = THIS_MODULE,
    .open       = iface_open,
    .read       = seq_read,
    .write      = iface_write,
    .llseek     = seq_lseek,
    .release    = single_release
};
#else
const static struct proc_ops iface_ops = {
    .proc_open       = iface_open,
    .proc_read       = seq_read,
    .proc_write      = iface_write,
    .proc_lseek     = seq_lseek,
    .proc_release    = single_release
};
#endif

int erdevice_proc_init(void)
{
    struct proc_dir_entry *proc;
    proc = proc_mkdir("edgerouter-status", NULL);
    if (!proc) {
        pr_err("can't create dir /proc/edgerouter-status/\n");
        return -ENODEV;;
    }
    proc_create("erdevices", 0644, proc, &erdevices_ops);
    proc_create("target_ip", 0644, proc, &target_ip_ops);
    proc_create("target_mac", 0644, proc, &target_mac_ops);
    proc_create("iface", 0644, proc, &iface_ops);
    proc_create("mode", 0644, proc, &mode_ops);
    return 0;
}


void erdevice_proc_remove(void)
{
    remove_proc_subtree("edgerouter-status", NULL);
}
