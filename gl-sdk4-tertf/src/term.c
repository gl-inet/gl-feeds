/*
 *  Copyright (C) 2019 jianhui zhao <zhaojh329@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/jhash.h>
#include <asm/unaligned.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/etherdevice.h>
#include <linux/netfilter.h>
#include <linux/inetdevice.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/inet.h>
#include <asm/div64.h>
#include <net/arp.h>
#include <linux/kobject.h>

#include "term.h"

#define TERM_HASH_SIZE              (1 << 8)
#define TERM_EVENT_BUF_SIZE         512

static struct kmem_cache *term_cache __read_mostly;
static struct hlist_head terms[TERM_HASH_SIZE];
static u32 hash_rnd __read_mostly;
static atomic64_t term_init_id;

/*
* keepalive_work:
* Before timeout, ICMP message is sent first. After the timeout time is reached, if the device is not offline,
* the time will be updated, otherwise the device will be offline.
*/
struct delayed_work keepalive_work;
struct delayed_work gc_work;

static unsigned long ttl;
static unsigned long icmp_ttl;

static DEFINE_SPINLOCK(hash_lock);

extern u64 uevent_next_seqnum(void);

static int term_event_add_var(struct term_event *event, int argv,
                              const char *format, ...)
{
    static char buf[128];
    char *s;
    va_list args;
    int len;

    if (argv)
        return 0;

    va_start(args, format);
    len = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (len >= sizeof(buf)) {
        WARN_ON(1);
        return -ENOMEM;
    }

    s = skb_put(event->skb, len + 1);
    strcpy(s, buf);

    return 0;
}

static const char *event_action_str(int act)
{
    switch (act) {
        case TERM_EVENT_ADD:
            return "add";
        case TERM_EVENT_DEL:
            return "del";
        case TERM_EVENT_CHANGE:
            return "chg";
        default:
            return NULL;
    }
}

static int hotplug_fill_event(struct term_event *event, const char *action)
{
    int ret;

    ret = term_event_add_var(event, 0, "HOME=%s", "/");
    if (ret)
        return ret;

    ret = term_event_add_var(event, 0, "PATH=%s",
                             "/sbin:/bin:/usr/sbin:/usr/bin");
    if (ret)
        return ret;

    ret = term_event_add_var(event, 0, "SUBSYSTEM=%s", "gl-tertf");
    if (ret)
        return ret;

    ret = term_event_add_var(event, 0, "ACTION=%s", action);
    if (ret)
        return ret;

    ret = term_event_add_var(event, 0, "MAC=%pM", event->mac);
    if (ret)
        return ret;

    ret = term_event_add_var(event, 0, "IP=%pI4", &event->ip);
    if (ret)
        return ret;

    if (event->action == TERM_EVENT_ADD) {
        ret = term_event_add_var(event, 0, "DEV=%s", event->dev);
        if (ret)
            return ret;
    }

    ret = term_event_add_var(event, 0, "SEQNUM=%llu", uevent_next_seqnum());

    return ret;
}

static void hotplug_work(struct work_struct *work)
{
    struct term_event *event = container_of(work, struct term_event, work);
    const char *action = event_action_str(event->action);
    int ret = 0;

    if (!action)
        goto out_free_event;

    event->skb = alloc_skb(TERM_EVENT_BUF_SIZE, GFP_KERNEL);
    if (!event->skb)
        goto out_free_event;

    ret = term_event_add_var(event, 0, "%s@", action);
    if (ret)
        goto out_free_skb;

    ret = hotplug_fill_event(event, action);
    if (ret)
        goto out_free_skb;

    NETLINK_CB(event->skb).dst_group = 1;
    broadcast_uevent(event->skb, 0, 1, GFP_KERNEL);

out_free_skb:
    if (ret) {
        kfree_skb(event->skb);
    }
out_free_event:
    kfree(event);
}

static void hotplug_create_event(struct terminal *term, int action)
{
    struct term_event *event;

    event = kzalloc(sizeof(struct term_event), GFP_KERNEL);
    if (!event)
        return;

    event->ip = term->ip;

    memcpy(event->mac, term->mac, ETH_ALEN);

    if (action == TERM_EVENT_ADD)
        strcpy(event->dev, netdev_name(term->dev));

    event->action = action;

    INIT_WORK(&event->work, hotplug_work);
    schedule_work(&event->work);
}

static inline int term_mac_hash(const u8 *mac)
{
    /* use 1 byte of OUI and 3 bytes of NIC */
    u32 key = get_unaligned((u32 *)(mac + 2));
    return jhash_1word(key, hash_rnd) & (TERM_HASH_SIZE - 1);
}

static void term_rcu_free(struct rcu_head *head)
{
    struct terminal *term = container_of(head, struct terminal, rcu);

#if 0
    pr_info("term: %pM %pI4 freed\n", term->mac, &term->ip);
#endif

    hotplug_create_event(term, TERM_EVENT_DEL);

    dev_put(term->dev);
    free_percpu(term->stats);
    kmem_cache_free(term_cache, term);
}

static void term_delete(struct terminal *term)
{
    hlist_del_rcu(&term->hlist);
    call_rcu(&term->rcu, term_rcu_free);
}

static struct terminal *find_term_rcu(struct hlist_head *head, const u8 *mac)
{
    struct terminal *term;

    hlist_for_each_entry_rcu(term, head, hlist) {
        if (ether_addr_equal(term->mac, mac))
            return term;
    }

    return NULL;
}

static struct terminal *find_term(struct hlist_head *head, const u8 *mac)
{
    struct terminal *term;

    hlist_for_each_entry(term, head, hlist) {
        if (ether_addr_equal(term->mac, mac))
            return term;
    }

    return NULL;
}

void term_create(const u8 *mac, __be32 addr, struct net_device *dev)
{
    struct hlist_head *head = &terms[term_mac_hash(mac)];
    struct terminal *term, *old;
    int i;

    term = find_term_rcu(head, mac);
    if (likely(term && term->dev == dev))
        return;

    term = kmem_cache_zalloc(term_cache, GFP_ATOMIC);
    if (!term)
        return;

    term->stats = alloc_percpu_gfp(struct term_stats, GFP_ATOMIC);
    if (!term->stats) {
        kmem_cache_free(term_cache, term);
        return;
    }

    for_each_possible_cpu(i) {
        struct term_stats *st = per_cpu_ptr(term->stats, i);
        u64_stats_init(&st->syncp);
    }

    dev_hold(dev);

    term->id = atomic64_inc_return(&term_init_id);
    term->ip = addr;
    term->dev = dev;
    memcpy(term->mac, mac, ETH_ALEN);

#if 0
    pr_info("term: %pM %pI4 added\n", term->mac, &term->ip);
#endif

    hotplug_create_event(term, TERM_EVENT_ADD);

    spin_lock(&hash_lock);
    old = find_term(head, mac);
    if (unlikely(old)) {
        hlist_replace_rcu(&old->hlist, &term->hlist);
        call_rcu(&old->rcu, term_rcu_free);
    } else {
        hlist_add_head_rcu(&term->hlist, head);
    }
    spin_unlock(&hash_lock);
}

void term_update(const u8 *mac, __be32 addr, unsigned int rx, unsigned int tx, bool alive)
{
    struct hlist_head *head = &terms[term_mac_hash(mac)];
    struct terminal *term;
    struct term_stats *st;

    term = find_term_rcu(head, mac);
    if (!term)
        return;

    if (alive)
        term->updated = jiffies;

    if (unlikely(addr != term->ip)) {
        term->ip = addr;
        hotplug_create_event(term, TERM_EVENT_CHANGE);
    } else {
        term->ip = addr;
    }

    st = this_cpu_ptr(term->stats);

    u64_stats_update_begin(&st->syncp);
    st->rx_bytes += rx;
    st->tx_bytes += tx;
    u64_stats_update_end(&st->syncp);
}
EXPORT_SYMBOL(term_update);

static void term_update2(s64 id, unsigned int rx, unsigned int tx)
{
    struct terminal *term;
    int i;

    for (i = 0; i < TERM_HASH_SIZE; i++) {
        rcu_read_lock();
        hlist_for_each_entry_rcu(term, &terms[i], hlist) {
            if (term->id == id) {
                struct term_stats *st = this_cpu_ptr(term->stats);
                term->updated = jiffies;
                u64_stats_update_begin(&st->syncp);
                st->rx_bytes += rx;
                st->tx_bytes += tx;
                u64_stats_update_end(&st->syncp);
                rcu_read_unlock();
                return;
            }
        }
        rcu_read_unlock();
    }
}

static void term_flush(void)
{
    struct terminal *term;
    struct hlist_node *n;
    int i;

    for (i = 0; i < TERM_HASH_SIZE; i++) {
        spin_lock_bh(&hash_lock);
        hlist_for_each_entry_safe(term, n, &terms[i], hlist) {
            term_delete(term);
        }
        spin_unlock_bh(&hash_lock);
    }
}

void flush_term_by_dev(struct net_device *dev)
{
    struct terminal *term;
    struct hlist_node *n;
    int i;

    for (i = 0; i < TERM_HASH_SIZE; i++) {
        spin_lock_bh(&hash_lock);
        hlist_for_each_entry_safe(term, n, &terms[i], hlist) {
            if (dev == term->dev)
                term_delete(term);
        }
        spin_unlock_bh(&hash_lock);
    }
}

static int proc_show(struct seq_file *s, void *v)
{
    struct terminal *term;
    int i;

    seq_printf(s, "%-17s  %-16s  %-16s  %-16s  %-16s %s\n", "MAC", "IP", "Tx(Byte)", "Rx(Byte)", "Device", "ID");

    for (i = 0; i < TERM_HASH_SIZE; i++) {
        rcu_read_lock();
        hlist_for_each_entry_rcu(term, &terms[i], hlist) {
            u64 tx_bytes_sum = 0, rx_bytes_sum = 0;
            int j;

            for_each_possible_cpu(j) {
                struct term_stats *st = per_cpu_ptr(term->stats, j);
                u64 tx_bytes, rx_bytes;
                unsigned int start;

                do {
                    start = u64_stats_fetch_begin_irq(&st->syncp);
                    tx_bytes = st->tx_bytes;
                    rx_bytes = st->rx_bytes;
                } while (u64_stats_fetch_retry_irq(&st->syncp, start));

                tx_bytes_sum += tx_bytes;
                rx_bytes_sum += rx_bytes;
            }

            seq_printf(s, "%pM  %-16pI4  %-16llu  %-16llu  %-16s %lld\n",
                       term->mac, &term->ip, tx_bytes_sum, rx_bytes_sum, netdev_name(term->dev), term->id);
        }
        rcu_read_unlock();
    }

    return 0;
}

static ssize_t proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    char data[128] = "";
    char *e;

    if (size > sizeof(data) - 1)
        return -EINVAL;

    if (copy_from_user(data, buf, size))
        return -EFAULT;

    e = strchr(data, '\n');
    if (e)
        *e = 0;

    if (data[0] == 'c') {
        term_flush();
        return size;
    }

    /* update traffic */
    if (data[0] == 'u') {
        u32 id, rx, tx;
        sscanf(data + 2, "%u %u %u", &id, &rx, &tx);
        term_update2(id, rx, tx);
    }

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

static void term_keepalive(struct work_struct *work)
{
    static char target_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    struct terminal *term;
    int i;

    for (i = 0; i < TERM_HASH_SIZE; i++) {
        rcu_read_lock();
        hlist_for_each_entry_rcu(term, &terms[i], hlist) {
            struct net_device *dev = term->dev;
            struct net_device *br_dev;
            struct in_device *indev;

            br_dev = netdev_master_upper_dev_get_rcu(term->dev);
            if (br_dev)
                dev = br_dev;

            indev = __in_dev_get_rcu(dev);
            if (indev) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
                for_primary_ifa(indev) {
#else
                const struct in_ifaddr *ifa;
                in_dev_for_each_ifa_rcu(ifa, indev) {
#endif
                    arp_send(ARPOP_REQUEST, ETH_P_ARP, term->ip, dev, ifa->ifa_address, target_mac, dev->dev_addr, target_mac);
                }
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
                endfor_ifa(indev);
#endif
            }
        }
        rcu_read_unlock();
    }

    mod_delayed_work(system_long_wq, &keepalive_work,  icmp_ttl);
}

static void term_cleanup(struct work_struct *work)
{
    unsigned long work_delay = ttl;
    struct terminal *term;
    struct hlist_node *n;
    int i;

    for (i = 0; i < TERM_HASH_SIZE; i++) {
        spin_lock_bh(&hash_lock);
        hlist_for_each_entry_safe(term, n, &terms[i], hlist) {
            if (time_after(jiffies, term->updated + ttl))
                term_delete(term);
            else
                work_delay = min(work_delay, term->updated + ttl - jiffies);
        }
        spin_unlock_bh(&hash_lock);
    }

    /* Cleanup minimum 10 milliseconds apart */
    work_delay = max_t(unsigned long, work_delay, msecs_to_jiffies(10));
    mod_delayed_work(system_long_wq, &gc_work, work_delay);
}

void set_term_ttl(unsigned long t)
{
    if (t > 0) {
        icmp_ttl = t * HZ / 3;//每个TTL周期内发送3个心跳包，如果丢包率高可以调整
        ttl = t * HZ;
    }

    mod_delayed_work(system_long_wq, &keepalive_work, 0);
    mod_delayed_work(system_long_wq, &gc_work, 0);
}

unsigned long get_term_ttl(void)
{
    return ttl / HZ;
}

int term_init(struct proc_dir_entry *proc)
{
    term_cache = kmem_cache_create("term_cache", sizeof(struct terminal), 0, 0, NULL);
    if (!term_cache)
        return -ENOMEM;

    proc_create("term", 0644, proc, &proc_ops);

    get_random_bytes(&hash_rnd, sizeof(hash_rnd));

    INIT_DELAYED_WORK(&keepalive_work, term_keepalive);
    INIT_DELAYED_WORK(&gc_work, term_cleanup);

    set_term_ttl(60);

    return 0;
}

void term_free(void)
{
    cancel_delayed_work_sync(&keepalive_work);
    cancel_delayed_work_sync(&gc_work);

    term_flush();

    rcu_barrier();  /* Wait for completion of call_rcu()'s */

    kmem_cache_destroy(term_cache);
}
