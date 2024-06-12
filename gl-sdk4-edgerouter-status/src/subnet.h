/*
 *  Copyright (C) 2019 jianhui zhao <zhaojh329@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef __SUBNET_
#define __SUBNET_

#include <linux/netdevice.h>

struct subnet {
    int ifindex;
    struct rcu_head rcu;
    struct hlist_node hlist;
    struct list_head list;
    char ifname[IFNAMSIZ];
};

bool subnet_exist(struct net_device *dev);
void del_subnet_dev(struct net_device *dev);
void add_subnet_dev(struct net_device *dev);

int subnet_init(struct proc_dir_entry *proc);
void subnet_free(void);

#endif
