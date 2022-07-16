#include <linux/version.h>
#include <linux/slab.h>
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
#include <linux/jiffies.h>

#include "erdevices.h"

static struct kmem_cache *devices_cache __read_mostly;
static u32 hash_rnd __read_mostly;
struct hlist_head devices[DEVICES_HASH_SIZE];


static inline int mac_hash(const u8 *mac)
{
    /* use 1 byte of OUI and 3 bytes of NIC */
    u32 key = get_unaligned((u32 *)(mac + 2));
    return jhash_1word(key, hash_rnd) & (DEVICES_HASH_SIZE - 1);
}

/*static __kernel_time_t jiffiestosec(const unsigned long jiffies)
{
    struct timeval value;
    jiffies_to_timeval(jiffies,&value);
    return value.tv_sec;
}*/

static __kernel_time_t get_cur_time(void)
{
    struct timeval value;
    do_gettimeofday(&value);
    return value.tv_sec;
}

void delete_erdevice(struct erdevice *device)
{
    hlist_del(&device->hlist);
}

void show_erdevice(struct seq_file *s)
{
    int i;
    struct erdevice *device;
    for (i = 0; i < DEVICES_HASH_SIZE ; i++) {
        hlist_for_each_entry(device, &devices[i], hlist) {
            seq_printf(s, "%pM\t%pI4\t%-16lu\t%-16lu\t%d\n", device->mac, (void *)&device->ip, device->lastalive, device->arpalive, device->debounce);
        }

    }
}

void flush_devices(void)
{
    int i;
    struct erdevice *device;
    struct hlist_node *n;

    for (i = 0; i < DEVICES_HASH_SIZE ; i++) {
        //spin_lock(&hash_lock);
        hlist_for_each_entry_safe(device, n, &devices[i], hlist) {
            delete_erdevice(device);
        }
        //spin_unlock(&hash_lock);
    }
}

static struct erdevice *find_erdevice(struct hlist_head *head, const u8 *mac)
{
    struct erdevice *erdevice;
    //spin_lock(&hash_lock);
    hlist_for_each_entry(erdevice, head, hlist) {
        if (ether_addr_equal(erdevice->mac, mac)) {
            //spin_unlock(&hash_lock);
            return erdevice;
        }
    }
    //spin_unlock(&hash_lock);

    return NULL;
}


int update_erdevices(const u8 *mac, const __be32 ip)
{
    struct hlist_head *head = &devices[mac_hash(mac)];
    struct erdevice *device;
    int ret = 0;

    spin_lock(&hash_lock);
    device = find_erdevice(head, mac);
    /* 如果设备存在，更新活跃时间和IP */
    if (likely(device)) {
        device->lastalive = get_cur_time();
        device->ip = ip;
        goto out;
    }

    if (erdevice_work_mode != 0) { //手动模式不自动增加设备
        goto out;
    }

    /* 如果设备不存在，创建设备 */
    device = kmem_cache_zalloc(devices_cache, GFP_ATOMIC);
    if (!device) {
        ret = -ENOMEM;
        goto out;
    }
    memcpy(device->mac, mac, ETH_ALEN);
    device->ip = ip;
    device->lastalive = get_cur_time();
    hlist_add_head(&device->hlist, head);
out:
    spin_unlock(&hash_lock);
    return ret;
}

int update_arp_alive(const u8 *mac, const __be32 ip)
{
    struct hlist_head *head = &devices[mac_hash(mac)];
    struct erdevice *device;
    int ret = 0;

    spin_lock(&hash_lock);
    device = find_erdevice(head, mac);
    /* 如果设备存在，更新活跃时间和IP */
    if (likely(device)) {
        if (device->debounce) { //离线设备重新上线消抖
            device->debounce--;
        }
        device->arpalive = 3;
        device->ip = ip;
        goto out;
    }

    if (erdevice_work_mode != 0) { //手动模式不自动增加设备
        goto out;
    }

    /* 如果设备不存在，创建设备 */
    device = kmem_cache_zalloc(devices_cache, GFP_ATOMIC);
    if (!device) {
        ret = -ENOMEM;
        goto out;
    }
    memcpy(device->mac, mac, ETH_ALEN);
    device->ip = ip;
    device->lastalive = 0;
    device->arpalive = 3;
    hlist_add_head(&device->hlist, head);
out:
    spin_unlock(&hash_lock);
    return ret;
}

int new_erdevices(const u8 *mac)
{
    struct hlist_head *head = &devices[mac_hash(mac)];
    struct erdevice *device;
    spin_lock(&hash_lock);
    int ret = 0;

    device = find_erdevice(head, mac);
    if (likely(device)) {
        goto out;
    }

    /* 如果设备不存在，创建设备 */
    device = kmem_cache_zalloc(devices_cache, GFP_ATOMIC);
    if (!device) {
        ret = -ENOMEM;
        goto out;
    }
    memcpy(device->mac, mac, ETH_ALEN);
    device->lastalive = 0;
    device->arpalive = 0;
    hlist_add_head(&device->hlist, head);
out:
    spin_unlock(&hash_lock);
    return ret;

}

int devices_init(void)
{
    devices_cache = kmem_cache_create("devices_cache", sizeof(struct erdevice), 0, 0, NULL);
    if (!devices_cache)
        return -ENOMEM;

    get_random_bytes(&hash_rnd, sizeof(hash_rnd));


    return 0;
}


void devices_remove(void)
{
    kmem_cache_destroy(devices_cache);
}
