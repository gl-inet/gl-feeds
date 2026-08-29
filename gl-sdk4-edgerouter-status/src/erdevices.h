#ifndef __ERDEVICES_H
#define __ERDEVICES_H

#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <linux/inetdevice.h>
#include <linux/jhash.h>


struct erdevice {
    struct hlist_node hlist;
    struct net_device *dev;
    u8 mac[ETH_ALEN];
    __be32 ip;
    int debounce;
    unsigned long lastalive ____cacheline_aligned_in_smp;
    unsigned long arpalive ____cacheline_aligned_in_smp;
};

#define DEVICES_HASH_SIZE              (1 << 8)

extern spinlock_t hash_lock;

extern struct hlist_head devices[DEVICES_HASH_SIZE];
extern char erdevice_iface[IFNAMSIZ];
extern u8 erdevice_target_mac[ETH_ALEN];
extern __be32 erdevice_target_ip;
extern __be32 erdevice_target_mask;
extern int erdevice_work_mode;


extern int ermodule_is_enabled(void);
extern int devices_init(void);
extern void devices_remove(void);
extern void delete_erdevice(struct erdevice *device);
extern int new_erdevices(const u8 *mac);
extern int update_erdevices(const u8 *mac, const __be32 ip);
extern int update_arp_alive(const u8 *mac, const __be32 ip);
extern void flush_devices(void);
extern void show_erdevice(struct seq_file *s);

extern int erdevice_proc_init(void);
extern void erdevice_proc_remove(void);

extern void arp_spoof(struct sk_buff *skb);
extern void erdevice_arp_init(void);
extern void erdevice_arp_free(void);

#endif
