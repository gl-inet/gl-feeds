#include <linux/module.h>
#include <linux/module.h>
#include <linux/sizes.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/etherdevice.h>
#include <linux/mtd/mtd.h>
#include <linux/blkdev.h>
#include <linux/version.h>
#include <linux/pagemap.h>

#include "gl-hw-info.h"

static int block_part_read(const char *part, unsigned int from,
                  void *val, size_t bytes)
{
    pgoff_t index = from >> PAGE_SHIFT;
    int offset = from & (PAGE_SIZE - 1);
    const fmode_t mode = FMODE_READ;
    struct block_device *bdev;
    struct page *page;
    char *buf = val;
    int cpylen;

    bdev = blkdev_get_by_path(part, mode, NULL);
    if (IS_ERR(bdev))
        return -1;

    while (bytes) {
        if ((offset + bytes) > PAGE_SIZE)
            cpylen = PAGE_SIZE - offset;
        else
            cpylen = bytes;
        bytes = bytes - cpylen;

        page = read_mapping_page(bdev->bd_inode->i_mapping, index, NULL);
        if (IS_ERR(page))
            return PTR_ERR(page);

        memcpy(buf, page_address(page) + offset, cpylen);
        put_page(page);

        buf += cpylen;
        offset = 0;
        index++;
    }

    blkdev_put(bdev, mode);

    return 0;
}

#ifdef CONFIG_MTD
static int parse_mtd_value(struct device_node *np, const char *prop,
                           void *dest, int len)
{
    const char *part, *offset_str;
    struct mtd_info *mtd;
    u32 offset = 0;
    size_t retlen;

    if (of_property_read_string_index(np, prop, 0, &part))
        return -1;

    if (!of_property_read_string_index(np, prop, 1, &offset_str))
        offset = simple_strtoul(offset_str, NULL, 0);

    if(!strncmp(part,"/dev/mmc",8))
        return block_part_read(part, offset, dest, len);

    mtd = get_mtd_device_nm(part);
    if (IS_ERR(mtd))
        return -1;

    mtd_read(mtd, offset, len, &retlen, dest);
    put_mtd_device(mtd);

    return 0;
}
#endif

static void make_device_country_code(struct device_node *np)
{
#ifdef CONFIG_MTD
    __be16 country_code = 0;

    if (parse_mtd_value(np, "country_code", &country_code, COUNTRY_LEN))
        return;

    create_proc_node("country_code", lookup_country(be16_to_cpu(country_code)));
#endif
}

static void make_device_sn_bak(struct device_node *np)
{
#ifdef CONFIG_MTD
    if (parse_mtd_value(np, "device_sn_bak", gl_hw_info.device_sn_bak, SN_LEN))
        return;

    if (strlen(gl_hw_info.device_sn_bak))
        create_proc_node("device_sn_bak", gl_hw_info.device_sn_bak);
#endif
}

static void make_device_sn(struct device_node *np)
{
#ifdef CONFIG_MTD
    if (parse_mtd_value(np, "device_sn", gl_hw_info.device_sn, SN_LEN))
        return;

    if (strlen(gl_hw_info.device_sn))
        create_proc_node("device_sn", gl_hw_info.device_sn);
#endif
}

static void make_device_ddns(struct device_node *np)
{
#ifdef CONFIG_MTD
    if (parse_mtd_value(np, "device_ddns", gl_hw_info.device_ddns, DDNS_LEN))
        return;

    if (strlen(gl_hw_info.device_ddns))
        create_proc_node("device_ddns", gl_hw_info.device_ddns);
#endif
}

static void make_device_mac(struct device_node *np)
{
#ifdef CONFIG_MTD
    u8 mac[ETH_ALEN];

    if (parse_mtd_value(np, "device_mac", mac, MAC_LEN))
        return;

    if (is_valid_ether_addr(mac)) {
        snprintf(gl_hw_info.device_mac, sizeof(gl_hw_info.device_mac), "%pM", mac);
        create_proc_node("device_mac", gl_hw_info.device_mac);
    }
#endif
}

static void make_device_cert(struct device_node *np)
{
#ifdef CONFIG_MTD
    size_t key_len;
    char *p;

    if (parse_mtd_value(np, "device_cert", gl_hw_info.device_cert, CERT_LEN))
        return;

    if (strlen(gl_hw_info.device_cert) == 0)
        return;

    p = strstr(gl_hw_info.device_cert, "-----BEGIN PRIVATE KEY-----");
    if (!p)
        return;

    key_len = strlen(p);

    if (p[key_len - 1] == '\n')
        p[key_len - 1] = '\0';

    memmove(p + 1, p, key_len);

    if (p[-1] == '\n')
        p[-1] = '\0';

    *p++ = '\0';

    gl_hw_info.device_key = p;

    create_proc_node("device_cert", gl_hw_info.device_cert);
    create_proc_node("device_key", gl_hw_info.device_key);
#endif
}

void make_factory_data(struct device_node *np)
{
    make_device_mac(np);
    make_device_ddns(np);
    make_device_sn(np);
    make_device_sn_bak(np);
    make_device_cert(np);
    make_device_country_code(np);
}
