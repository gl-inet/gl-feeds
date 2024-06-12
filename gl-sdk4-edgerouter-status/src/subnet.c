/*
 *  Copyright (C) 2019 jianhui zhao <zhaojh329@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/rtnetlink.h>
#include <linux/proc_fs.h>
#include <linux/inet.h>

#include "subnet.h"

static LIST_HEAD(subnets);
static DEFINE_SPINLOCK(lock);

static struct hlist_head subnets_index[NETDEV_HASHENTRIES];

bool subnet_exist(struct net_device *dev)
{
    struct hlist_head *head = &subnets_index[dev->ifindex & (NETDEV_HASHENTRIES - 1)];
    struct subnet *n;

    hlist_for_each_entry_rcu(n, head, hlist) {
        if (n->ifindex == dev->ifindex)
            return true;
    }

    return false;
}

void del_subnet_dev(struct net_device *dev)
{
    struct hlist_head *head = &subnets_index[dev->ifindex & (NETDEV_HASHENTRIES - 1)];
    struct subnet *n;

    spin_lock_bh(&lock);
    hlist_for_each_entry(n, head, hlist) {
        if (n->ifindex == dev->ifindex) {
            n->ifindex = -1;
            hlist_del_rcu(&n->hlist);
            pr_info("tertf: %s changed, delete it\n", dev->name);
            break;
        }
    }
    spin_unlock_bh(&lock);
}

static void add_subnet_dev_nolock(struct net_device *dev)
{
    struct hlist_head *head = &subnets_index[dev->ifindex & (NETDEV_HASHENTRIES - 1)];
    struct subnet *net, *tmp;

    list_for_each_entry(net, &subnets, list) {
        if (!strcmp(net->ifname, dev->name)) {
            hlist_for_each_entry(tmp, head, hlist) {
                if (tmp == net)
                    return;
            }
            pr_info("tertf: %s registered, add it\n", dev->name);
            net->ifindex = dev->ifindex;
            hlist_add_head_rcu(&net->hlist, head);
            break;
        }
    }
}

void add_subnet_dev(struct net_device *dev)
{
    spin_lock_bh(&lock);
    add_subnet_dev_nolock(dev);
    spin_unlock_bh(&lock);
}

static int proc_show(struct seq_file *s, void *v)
{
    struct subnet *net;

    seq_printf(s, "ifindex ifname\n");

    spin_lock_bh(&lock);
    list_for_each_entry(net, &subnets, list) {
        seq_printf(s, "%-7d %s\n", net->ifindex, net->ifname);
    }
    spin_unlock_bh(&lock);

    return 0;
}

/*
** remove the spaces(carriage returns) at the brginning and end of the string
*/
static void clean_string(char *str)
{
    char *start = str - 1;
    char *end = str;
    char *p = str;

    while (*p) {
        switch (*p) {
            case ' ':
            case '\r':
            case '\n': {
                if (start + 1 == p)
                    start = p;
            }
            break;
            default:
                break;
        }
        ++p;
    }
    --p;
    ++start;
    if (*start == 0) {
        *str = 0;
        return;
    }
    end = p + 1;
    while (p > start) {
        switch (*p) {
            case ' ':
            case '\r':
            case '\n': {
                if (end - 1 == p)
                    end = p;
            }
            break;
            default:
                break;
        }
        --p;
    }
    memmove(str, start, end - start);
    *(str + (int)end - (int)start) = 0;
}

static void add_subnet(char *ifname)
{
    struct net_device *dev;
    struct subnet *net;

    spin_lock_bh(&lock);

    list_for_each_entry(net, &subnets, list) {
        if (!strcmp(net->ifname, ifname)) {
            pr_err("tertf: %s already exits\n", ifname);
            goto err;
        }
    }

    net = kzalloc(sizeof(struct subnet), GFP_ATOMIC);
    if (!net) {
        pr_err("tertf: no mem\n");
        goto err;
    }

    net->ifindex = -1;

    strcpy(net->ifname, ifname);
    list_add_tail(&net->list, &subnets);

    dev = dev_get_by_name(&init_net, ifname);
    if (dev) {
        add_subnet_dev_nolock(dev);
        dev_put(dev);
    }

err:
    spin_unlock_bh(&lock);
}

static ssize_t proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    char ifname[IFNAMSIZ] = {0};

    if (size > IFNAMSIZ - 1)
        return -EINVAL;

    if (copy_from_user(ifname, buf, size))
        return -EFAULT;

    clean_string(ifname);

    add_subnet(ifname);

    return size;
}

static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_show, NULL);
}

const static struct file_operations proc_ops = {
    .owner      = THIS_MODULE,
    .open       = proc_open,
    .read       = seq_read,
    .write      = proc_write,
    .llseek     = seq_lseek,
    .release    = single_release
};

int subnet_init(struct proc_dir_entry *proc)
{
    proc_create("subnet", 0644, proc, &proc_ops);

    return 0;
}


static void subnet_index_clear(void)
{
    struct subnet *n;
    struct hlist_node *tmp;
    int i;

    for (i = 0; i < NETDEV_HASHENTRIES; i++) {
        spin_lock_bh(&lock);
        hlist_for_each_entry_safe(n, tmp, &subnets_index[i], hlist) {
            hlist_del_rcu(&n->hlist);
        }
        spin_unlock_bh(&lock);
    }
}

static void subnet_rcu_free(struct rcu_head *head)
{
    struct subnet *n = container_of(head, struct subnet, rcu);
    kfree(n);
}

void subnet_free(void)
{
    struct subnet *n, *tmp;

    subnet_index_clear();

    spin_lock_bh(&lock);
    list_for_each_entry_safe(n, tmp, &subnets, list) {
        list_del(&n->list);
        call_rcu(&n->rcu, subnet_rcu_free);
    }
    spin_unlock_bh(&lock);
}
