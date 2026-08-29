#include <net/arp.h>
#include <linux/ip.h>
#include "erdevices.h"
#include <linux/workqueue.h>

static struct work_struct do_arp_work;
static struct delayed_work arp_loop_work;

struct arp_sender {
    struct work_struct work;
    __be32 sip;
    __be32 tip;
    u8 smac[ETH_ALEN];
    u8 tmac[ETH_ALEN];
    struct net_device *dev;
};
struct arp_sender *gl_arp_sender;

static void arp_send_special_sender(int type, int ptype, __be32 dest_ip,
                                    struct net_device *dev, __be32 src_ip,
                                    const unsigned char *dest_hw,
                                    const unsigned char *src_hw,
                                    const unsigned char *target_hw,
                                    const unsigned char *sender_hw)
{
    struct sk_buff *skb;
    unsigned char *ptr;
    struct arphdr *ahdr;

    /* arp on this interface. */
    if (dev->flags & IFF_NOARP)
        return;

    skb = arp_create(type, ptype, dest_ip, dev, src_ip,
                     dest_hw, src_hw, target_hw);
    if (!skb)
        return;
    ahdr = arp_hdr(skb);
    ptr = (unsigned char *)(ahdr + 1);
    memcpy(ptr, sender_hw, ETH_ALEN);
    arp_xmit(skb);
}

static void spoof(__be32 dest_ip, struct net_device *dev, const unsigned char *target_hw)
{
    char probe_mac[ETH_ALEN] = {0, 0, 0, 0, 0, 0};
    struct neighbour *n = __ipv4_neigh_lookup_noref(dev, erdevice_target_ip);

    if (n) {
        arp_send_special_sender(ARPOP_REQUEST, ETH_P_ARP, dest_ip, dev, 0, target_hw, dev->dev_addr, probe_mac, n->ha);
    }
    arp_send(ARPOP_REPLY, ETH_P_ARP, dest_ip, dev, erdevice_target_ip, target_hw, dev->dev_addr, target_hw);
    arp_send(ARPOP_REQUEST, ETH_P_ARP, dest_ip, dev, erdevice_target_ip, target_hw, dev->dev_addr, target_hw);
}

static void do_arp_spoof(struct work_struct *mywork)
{
    struct arp_sender *sender;

    sender = container_of(mywork, struct arp_sender, work);
    if (!sender)
        return;
    //spoof(sender->tip,sender->dev,sender->tmac);
    //arp_send(ARPOP_REQUEST, ETH_P_ARP, sender->tip, sender->dev, 0, probe_dmac, sender->smac, probe_mac);
    //arp_send(ARPOP_REPLY, ETH_P_ARP, sender->tip, sender->dev, sender->sip, sender->tmac, sender->smac, sender->tmac);

}

void arp_spoof(struct sk_buff *skb)
{
    unsigned char *ptr;
    __be32 sip, tip;
    struct ethhdr *ehdr = eth_hdr(skb);
    struct arphdr *ahdr = arp_hdr(skb);
    if (ehdr == NULL || ahdr == NULL)
        return;

    if (ahdr->ar_op != __cpu_to_be16(ARPOP_REPLY))
        return;

    ptr = (unsigned char *)(ahdr + 1);
    ptr += skb->dev->addr_len;
    memcpy(&sip, ptr, 4);
    ptr += 4;
    ptr += skb->dev->addr_len;
    memcpy(&tip, ptr, 4);

    //printk("erdevice recv arp reply %pM\t%pI4\n",ehdr->h_source,&sip);
    if ((!((sip ^ erdevice_target_ip) & erdevice_target_mask))
            && (sip != erdevice_target_ip)
            && (ahdr->ar_op == __cpu_to_be16(ARPOP_REPLY))) {
        update_arp_alive(ehdr->h_source, sip);
    }/* else if ((!((sip ^ erdevice_target_ip) & erdevice_target_mask))
               && (tip == erdevice_target_ip)
               && gl_arp_sender
               && (ahdr->ar_op == __cpu_to_be16(ARPOP_REQUEST))) {
        //spoof(sip,skb->dev,ehdr->h_source);
        gl_arp_sender->tip = sip;
        gl_arp_sender->sip = tip;
        memcpy(gl_arp_sender->tmac, ehdr->h_source, ETH_ALEN);
        memcpy(gl_arp_sender->smac, skb->dev->dev_addr, ETH_ALEN);
        gl_arp_sender->dev = skb->dev;
        schedule_work(&gl_arp_sender->work);
    }*/
    return;
}


static void do_arp_loop(struct work_struct *work)
{
    __be32 start, end, ifaddr;
    struct net_device *dev;
    char probe_mac[ETH_ALEN] = {0, 0, 0, 0, 0, 0};
    char probe_dmac[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    int i;
    struct erdevice *device;

    if (!ermodule_is_enabled())
        goto out;

    if (strlen(erdevice_iface))
        dev = dev_get_by_name(&init_net, erdevice_iface);
    if (!dev)
        goto out;

    rcu_read_lock();
    if (dev->ip_ptr && (dev->ip_ptr)->ifa_list)
        ifaddr = ((dev->ip_ptr)->ifa_list)->ifa_address;
    rcu_read_unlock();

    start = (erdevice_target_ip & erdevice_target_mask);
    end = start + (cpu_to_be32(0xffffffff) - erdevice_target_mask);

    for (start = start + cpu_to_be32(1); start < end;  start = start + cpu_to_be32(1)) {
        if (start == erdevice_target_ip)
            continue;

        arp_send(ARPOP_REQUEST, ETH_P_ARP, start, dev, ifaddr, probe_dmac, dev->dev_addr, probe_mac);
        //printk("%pM\t%pI4 send arp to %pM\t%pI4\n",dev->dev_addr, &(((dev->ip_ptr)->ifa_list)->ifa_address),probe_dmac,&start);
    }

    for (i = 0; i < DEVICES_HASH_SIZE ; i++) {
        hlist_for_each_entry(device, &devices[i], hlist) {
            //spin_lock(&hash_lock);
            if (device->arpalive == 0 && device->debounce == 0) { //设备里先设置防抖标志，等待设备文档获取IP后才重新spoof，消抖时间为3个周期
                device->debounce = 3;
            } else if (device->arpalive && device->debounce == 0) {
                device->arpalive = device->arpalive -  1;
                spoof(device->ip, dev, device->mac);
            }
            //spin_unlock(&hash_lock);
        }

    }
    dev_put(dev);
out:
    mod_delayed_work(system_long_wq, &arp_loop_work, HZ * 3);
}

void erdevice_arp_init(void)
{
    gl_arp_sender = (struct arp_sender *)kzalloc(sizeof(struct arp_sender), GFP_KERNEL);
    if (gl_arp_sender == NULL) {
        return;
    }
    gl_arp_sender->work = do_arp_work;
    INIT_WORK(&gl_arp_sender->work, do_arp_spoof);
    INIT_DELAYED_WORK(&arp_loop_work, do_arp_loop);
    mod_delayed_work(system_long_wq, &arp_loop_work, HZ * 3);
}

void erdevice_arp_free(void)
{
    if (gl_arp_sender) {
        kfree(gl_arp_sender);
    }
    cancel_delayed_work_sync(&arp_loop_work);
    return;
}
