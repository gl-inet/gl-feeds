/*
 *  Copyright (C) 2019 jianhui zhao <zhaojh329@gmail.com>
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
#include <linux/netfilter_bridge.h>
#include <net/netfilter/nf_conntrack.h>
#include <linux/netfilter/nf_conntrack_common.h>

#include "term.h"
#include "subnet.h"

#define IS_IP(skb) (!skb_vlan_tag_present(skb) && skb->protocol == htons(ETH_P_IP))

static int proc_show(struct seq_file *s, void *v)
{
    seq_printf(s, "ttl: %ld\n", get_term_ttl());

    return 0;
}

static ssize_t proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    char conf[128] = "";
    char *endp, *value;

    if (size > sizeof(conf) - 1)
        return -EINVAL;

    if (copy_from_user(conf, buf, size))
        return -EFAULT;

    value = strchr(conf, '=');
    if (!value)
        return -EINVAL;
    *value++ = 0;

    if (!strcmp(conf, "ttl")) {
        unsigned long t = simple_strtoul(value, &endp, 10);
        if (endp == value)
            return -EINVAL;
        set_term_ttl(t);
    } else {
        return -EINVAL;
    }

    return size;
}

static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_show, NULL);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
const static struct file_operations gl_proc_ops = {
    .owner      = THIS_MODULE,
    .open       = proc_open,
    .read       = seq_read,
    .write      = proc_write,
    .llseek     = seq_lseek,
    .release    = single_release
};
#else
const static struct proc_ops gl_proc_ops = {
    .proc_open       = proc_open,
    .proc_read       = seq_read,
    .proc_write      = proc_write,
    .proc_lseek     = seq_lseek,
    .proc_release    = single_release
};
#endif

static u32 oui_ipv4_forward_hook(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
    struct ethhdr *ehdr = eth_hdr(skb);
    struct iphdr *iph = ip_hdr(skb);
    enum ip_conntrack_info ctinfo;
    __be32 saddr, daddr;
    struct nf_conn *ct;

    saddr = iph->saddr;
    daddr = iph->daddr;

    ct = nf_ct_get(skb, &ctinfo);
    if (!ct)
        return NF_ACCEPT;

    if (CTINFO2DIR(ctinfo) == IP_CT_DIR_ORIGINAL) {
        term_update(ehdr->h_source, saddr, 0, skb->len, true);
    } else {
        struct neighbour *n = __ipv4_neigh_lookup_noref(state->out, daddr);
        if (n == NULL)
            return NF_ACCEPT;
        term_update(n->ha, daddr, skb->len, 0, false);
    }

    return NF_ACCEPT;
}

static u32 oui_arp_in_hook(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
    struct ethhdr *ehdr = eth_hdr(skb);
    unsigned char *arp_ptr;
    __be32 sip;

    if (!subnet_exist(skb->dev))
        return NF_ACCEPT;

    arp_ptr = (unsigned char *)(arp_hdr(skb) + 1);
    arp_ptr += skb->dev->addr_len;
    memcpy(&sip, arp_ptr, 4);

    term_update(ehdr->h_source, sip, 0, 0, true);

    return NF_ACCEPT;
}

static u32 oui_bridge_pre_routing_hook(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
    struct net_device *dev = netdev_master_upper_dev_get_rcu(skb->dev);
    struct ethhdr *ehdr = eth_hdr(skb);
    struct iphdr *iph;

    if (!IS_IP(skb))
        return NF_ACCEPT;

    if (!subnet_exist(dev))
        return NF_ACCEPT;

    ehdr = eth_hdr(skb);
    iph = ip_hdr(skb);

    if (iph->saddr)
        term_create(ehdr->h_source, iph->saddr, skb->dev);

    return NF_ACCEPT;
}

static struct nf_hook_ops oui_bwm_ops[] __read_mostly = {
    {
        .hook       = oui_ipv4_forward_hook,
        .pf         = NFPROTO_IPV4,
        .hooknum    = NF_INET_FORWARD,
        .priority   = NF_IP_PRI_LAST
    },
    {
        .hook       = oui_arp_in_hook,
        .pf         = NFPROTO_ARP,
        .hooknum    = NF_ARP_IN,
        .priority   = -1
    },
    {
        .hook       = oui_bridge_pre_routing_hook,
        .pf         = NFPROTO_BRIDGE,
        .hooknum    = NF_BR_PRE_ROUTING,
        .priority   = NF_BR_PRI_FIRST
    }
};

static int tertf_device_event(struct notifier_block *unused,
                              unsigned long event, void *ptr)
{
    struct net_device *dev = netdev_notifier_info_to_dev(ptr);

    switch (event) {
        case NETDEV_CHANGENAME:
        case NETDEV_UNREGISTER:
            del_subnet_dev(dev);
            flush_term_by_dev(dev);
            break;
        case NETDEV_REGISTER:
            add_subnet_dev(dev);
            break;
    }

    return NOTIFY_DONE;
}

static struct notifier_block tertf_notifier_block = {
    .notifier_call = tertf_device_event,
};

static int __init oui_tertf_init(void)
{
    struct proc_dir_entry *proc;
    int ret = 0;

    proc = proc_mkdir("oui-tertf", NULL);
    if (!proc) {
        pr_err("can't create dir /proc/oui-tertf/\n");
        return -ENODEV;;
    }

    proc_create("config", 0644, proc, &gl_proc_ops);

    subnet_init(proc);

    ret = term_init(proc);
    if (ret)
        goto subnet_free;

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 12, 14)
    ret = nf_register_net_hooks(&init_net, oui_bwm_ops, ARRAY_SIZE(oui_bwm_ops));
#else
    ret = nf_register_hooks(oui_bwm_ops, ARRAY_SIZE(oui_bwm_ops));
#endif
    if (ret < 0) {
        pr_err("can't register hook\n");
        goto term_free;
    }

    register_netdevice_notifier(&tertf_notifier_block);

    pr_info("gl-tertf: (C) 2021 jianhui zhao <jianhui.zhao@gl-inet.com>\n");

    return 0;

term_free:
    term_free();
subnet_free:
    subnet_free();

    remove_proc_subtree("oui-tertf", NULL);

    return ret;
}

static void __exit oui_tertf_exit(void)
{
    unregister_netdevice_notifier(&tertf_notifier_block);

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 12, 14)
    nf_unregister_net_hooks(&init_net, oui_bwm_ops, ARRAY_SIZE(oui_bwm_ops));
#else
    nf_unregister_hooks(oui_bwm_ops, ARRAY_SIZE(oui_bwm_ops));
#endif

    term_free();
    subnet_free();

    remove_proc_subtree("oui-tertf", NULL);
}

module_init(oui_tertf_init);
module_exit(oui_tertf_exit);

MODULE_AUTHOR("jianhui zhao <jianhui.zhao@gl-inet.com>");
MODULE_LICENSE("GPL");
