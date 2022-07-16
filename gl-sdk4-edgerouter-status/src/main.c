/*
 *  Copyright (C) 2022 Chongjun Luo <luochongjung@gl-inet.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <net/arp.h>
#include <linux/ip.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_arp.h>

#include "erdevices.h"

char erdevice_iface[IFNAMSIZ] = {0};
u8 erdevice_target_mac[ETH_ALEN] = {0};
__be32 erdevice_target_ip = 0;
__be32 erdevice_target_mask = 0;
int erdevice_work_mode = 0;

DEFINE_SPINLOCK(hash_lock);

#define IS_IP(skb) (!skb_vlan_tag_present(skb) && skb->protocol == htons(ETH_P_IP))

static int mac_is_zero(u8 mac[ETH_ALEN])
{
    int i = 0;
    for (i = 0; i < ETH_ALEN; i++) {
        if (mac[i] != 0)
            return 0;
    }
    return 1;
}

int ermodule_is_enabled(void)
{
    if (erdevice_target_ip == 0 || mac_is_zero(erdevice_target_mac) == 1)
        return 0;
    return 1;
}

static u32 edge_ipv4_forward_hook(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
    struct ethhdr *ehdr = eth_hdr(skb);
    struct iphdr *iph = ip_hdr(skb);
    __be32 saddr, daddr;

    saddr = iph->saddr;
    daddr = iph->daddr;

    if (!ermodule_is_enabled())
        return NF_ACCEPT;

    if ((!((saddr ^ erdevice_target_ip) & erdevice_target_mask))
            && ether_addr_equal(erdevice_target_mac, ehdr->h_dest)
            && (saddr != erdevice_target_ip)) {
        update_erdevices(ehdr->h_source, saddr);
    }

    return NF_ACCEPT;
}

static u32 edge_arp_in_hook(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
    arp_spoof(skb);
    return NF_ACCEPT;
}

static struct nf_hook_ops edge_bwm_ops[] __read_mostly = {
    {
        .hook       = edge_ipv4_forward_hook,
        .pf         = NFPROTO_IPV4,
        .hooknum    = NF_INET_FORWARD,
        .priority   = NF_IP_PRI_FIRST,
    },
    {
        .hook       = edge_arp_in_hook,
        .pf         = NFPROTO_ARP,
        .hooknum    = NF_ARP_IN,
        .priority   = NF_IP_PRI_FIRST,
    },
};


static int __init edge_tertf_init(void)
{
    int ret = 0;

    erdevice_arp_init();

    ret = devices_init();
    if (ret < 0) {
        pr_err("can't devices list\n");
        return ret;
    }

    ret = erdevice_proc_init();
    if (ret < 0) {
        pr_err("can't create proc fs\n");
        goto free_devices;
    }

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 12, 14)
    ret = nf_register_net_hooks(&init_net, edge_bwm_ops, ARRAY_SIZE(edge_bwm_ops));
#else
    ret = nf_register_hooks(edge_bwm_ops, ARRAY_SIZE(edge_bwm_ops));
#endif
    if (ret < 0) {
        pr_err("can't register hook\n");
        goto free_proc;
    }

    return 0;
free_proc:
    erdevice_proc_remove();
free_devices:
    devices_remove();
    erdevice_arp_free();
    return ret;
}

static void __exit edge_tertf_exit(void)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 12, 14)
    nf_unregister_net_hooks(&init_net, edge_bwm_ops, ARRAY_SIZE(edge_bwm_ops));
#else
    nf_unregister_hooks(edge_bwm_ops, ARRAY_SIZE(edge_bwm_ops));
#endif
    erdevice_proc_remove();
    devices_remove();
    erdevice_arp_free();
}

module_init(edge_tertf_init);
module_exit(edge_tertf_exit);

MODULE_AUTHOR("Chongjun Luo <luochongjun@gl-inet.com>");
MODULE_LICENSE("GPL");
