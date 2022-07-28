/*
 *  Copyright (C) 2019 jianhui zhao <zhaojh329@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef __TERM_
#define __TERM_

#include <linux/if_ether.h>

#include "subnet.h"

struct term_stats {
    u64 rx_bytes;
    u64 tx_bytes;
    struct u64_stats_sync syncp;
};

struct terminal {
    struct hlist_node hlist;
    struct net_device *dev;
    unsigned long updated;
    struct rcu_head rcu;
    u8 mac[ETH_ALEN];
    __be32 ip;
    s64 id;
    struct term_stats __percpu *stats;
};

enum {
    TERM_EVENT_ADD,
    TERM_EVENT_DEL,
    TERM_EVENT_CHANGE
};

struct term_event {
    int action;
    __be32 ip;
    u8 mac[ETH_ALEN];
    char dev[IFNAMSIZ];
    struct sk_buff *skb;
    struct work_struct work;
};

void term_update(const u8 *mac, __be32 addr, unsigned int rx, unsigned int tx, bool alive);
void term_create(const u8 *mac, __be32 addr, struct net_device *dev);

void set_term_ttl(unsigned long t);
unsigned long get_term_ttl(void);

int term_init(struct proc_dir_entry *proc);
void term_free(void);

void flush_term_by_dev(struct net_device *dev);

#endif
