/*
 * 与 factory_data.c 对应：仅将 of_property_read_string_index 换为从 sn2.data 解析。
 * 原始 sn.data 行式解析仍由 gl_find_hw_info() 负责（见 main.c）。
 *
 * sn2.data 格式：
 *   板级：flash_size=128 / model=… / lan=… / wan=…
 *   工厂：factory_data.<属性>=<mtd名或/dev/…> [<偏移>]
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/sizes.h>
#include <linux/etherdevice.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/ubi.h>
#include <linux/blkdev.h>
#include <linux/version.h>
#include <linux/pagemap.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "gl-hw-info.h"

/* 与 GL_SN_DATA_FACTORY_PREFIX 拼接为 "factory_data.device_mac" 等 */
#define SNFD GL_SN_DATA_FACTORY_PREFIX

static char *sn2_data_buf;

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

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 5, 0)
    bdev = blkdev_get_by_path(part, mode, NULL);
#else
    bdev = blkdev_get_by_path(part, mode, NULL, NULL);
#endif
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 5, 0)
    blkdev_put(bdev, mode);
#else
    blkdev_put(bdev, NULL);
#endif

    return 0;
}

static int parse_mtd_value(const char *part, u32 offset, void *dest, int len)
{
#ifdef CONFIG_MTD
    struct mtd_info *mtd;
    size_t retlen;
    int rc;

    mtd = get_mtd_device_nm(part);
    if (IS_ERR(mtd))
        return -1;

    rc = mtd_read(mtd, offset, len, &retlen, dest);
    if (!rc && retlen != len)
        rc = -EIO;

    put_mtd_device(mtd);
    return rc;
#else
    return -ENODEV;
#endif
}

static int parse_ubi_value(const char *part, u32 offset, void *dest, int len)
{
#ifdef CONFIG_MTD_UBI
    struct ubi_volume_desc *desc;
    size_t retlen;
    int rc = 0;

    desc = ubi_open_volume_path(part, UBI_READONLY);
    if (IS_ERR(desc))
        return -1;
    retlen = ubi_read(desc, 0, dest, offset, len);
    if (retlen)
        rc = -EIO;

    ubi_close_volume(desc);
    return rc;
#else
    return -ENODEV;
#endif
}

static int ensure_sn2_data_loaded(void)
{
    struct file *file;
    loff_t offset = 0;
    ssize_t br;

    if (sn2_data_buf)
        return 0;

    sn2_data_buf = kmalloc(GL_DEVICE_BUFFER_SIZE, GFP_KERNEL);
    if (!sn2_data_buf) {
        pr_err("gl-hw-info: sn2.data kmalloc(%u) failed\n", GL_DEVICE_BUFFER_SIZE);
        return -ENOMEM;
    }

    file = filp_open(GL_DEVICE_FILE_PATH_SN2, O_RDONLY, 0);
    if (IS_ERR(file)) {
        long e = PTR_ERR(file);

        kfree(sn2_data_buf);
        sn2_data_buf = NULL;
        if (e == -ENOENT)
            return -ENOENT;
        pr_err("gl-hw-info: open %s failed, err=%ld\n", GL_DEVICE_FILE_PATH_SN2, e);
        return e;
    }

    br = kernel_read(file, sn2_data_buf, GL_DEVICE_BUFFER_SIZE - 1, &offset);
    filp_close(file, NULL);
    if (br < 0) {
        pr_err("gl-hw-info: read %s failed, ret=%zd\n", GL_DEVICE_FILE_PATH_SN2, br);
        kfree(sn2_data_buf);
        sn2_data_buf = NULL;
        return -EIO;
    }
    sn2_data_buf[br] = '\0';
    return 0;
}

/* 对应 of_property_read_string_index(np, prop, 0/1, ...) */
static int parse_value(const char *prop, void *dest, int len)
{
    const char *part;
    const char *offset_str = NULL;
    u32 offset = 0;
    char *line_buf;
    char *sp;
    const char *p;
    size_t plen = strlen(prop);

    if (ensure_sn2_data_loaded())
        return -1;

    p = sn2_data_buf;
    while (*p) {
        const char *nl = strchrnul(p, '\n');
        size_t linelen = nl - p;

        if (linelen > plen + 1 && !strncmp(p, prop, plen) && p[plen] == '=') {
            const char *val = p + plen + 1;
            size_t vlen = linelen - (plen + 1);

            while (vlen && (val[vlen - 1] == '\r' || val[vlen - 1] == '\n'))
                vlen--;

            line_buf = kmalloc(vlen + 1, GFP_KERNEL);
            if (!line_buf)
                return -1;
            memcpy(line_buf, val, vlen);
            line_buf[vlen] = '\0';
            strim(line_buf);

            part = line_buf;
            sp = strchr(line_buf, ' ');
            if (sp) {
                *sp++ = '\0';
                strim(sp);
                offset_str = sp;
            }
            if (offset_str)
                offset = simple_strtoul(offset_str, NULL, 0);

            if (!*part) {
                kfree(line_buf);
                return -1;
            }

            if (!strncmp(part, "/dev/ubi", 8)) {
                int r = parse_ubi_value(part, offset, dest, len);

                kfree(line_buf);
                return r;
            }

            if (!strncmp(part, "/dev/mmc", 8)) {
                int r = block_part_read(part, offset, dest, len);

                kfree(line_buf);
                return r;
            }

            {
                int r = parse_mtd_value(part, offset, dest, len);

                kfree(line_buf);
                return r;
            }
        }
        p = *nl ? nl + 1 : nl;
    }

    return -1;
}

static void make_device_country_code(void)
{
    __be16 country_code = 0;

    if (parse_value(SNFD "country_code", &country_code, COUNTRY_LEN))
        return;

    create_proc_node("country_code", lookup_country(be16_to_cpu(country_code)));
}

static void make_usb_enable_code(void)
{
    if (parse_value(SNFD "usb_power_mode", &gl_hw_info.usb_power_mode, USB_POWER_ENABLE))
        return;

    create_proc_node("usb_power_mode", &gl_hw_info.usb_power_mode);
}

static void make_device_sn_bak(void)
{
    if (parse_value(SNFD "device_sn_bak", gl_hw_info.device_sn_bak, SN_LEN))
        return;

    if (strlen(gl_hw_info.device_sn_bak))
        create_proc_node("device_sn_bak", gl_hw_info.device_sn_bak);
}

static void make_device_sn(void)
{
    if (parse_value(SNFD "device_sn", gl_hw_info.device_sn, SN_LEN))
        return;

    if (strlen(gl_hw_info.device_sn))
        create_proc_node("device_sn", gl_hw_info.device_sn);
}

static void make_device_ddns(void)
{
    if (parse_value(SNFD "device_ddns", gl_hw_info.device_ddns, DDNS_LEN))
        return;

    if (strlen(gl_hw_info.device_ddns))
        create_proc_node("device_ddns", gl_hw_info.device_ddns);
}

static void make_device_mac(void)
{
    u8 mac[ETH_ALEN];

    if (parse_value(SNFD "device_mac", mac, MAC_LEN))
        return;

    if (is_valid_ether_addr(mac)) {
        snprintf(gl_hw_info.device_mac, sizeof(gl_hw_info.device_mac), "%pM", mac);
        create_proc_node("device_mac", gl_hw_info.device_mac);
    }
}

static void make_device_cert(void)
{
    size_t key_len;
    char *p;

    if (parse_value(SNFD "device_cert", gl_hw_info.device_cert, CERT_LEN))
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
}

static void make_device_submodel(void)
{
    size_t submodel_len = 0;
    size_t i;
    bool all_ff = true;
    char *p;

    if (parse_value(SNFD "device_submodel", gl_hw_info.device_submodel, SUBMODEL_LEN))
        return;

    p = gl_hw_info.device_submodel;

    for (i = 0; i < SUBMODEL_LEN; i++) {
        if (p[i] != 0xffffffff || p[i] != 0x00) {
            all_ff = false;
            break;
        }
    }

    if (!all_ff) {
        for (i = 0; i < SUBMODEL_LEN; i++) {
            if (p[i] == 0xffffffff || p[i] == 0x00) {
                submodel_len = i;
                break;
            }
        }
        strncpy(gl_hw_info.device_submodel, p, submodel_len);
        gl_hw_info.device_submodel[submodel_len] = '\0';
    }

    create_proc_node("device_submodel", gl_hw_info.device_submodel);
}

static void make_firmware_type_flag(void)
{
    if (parse_value(SNFD "firmware_type", gl_hw_info.firmware_type, FIRMWARE_LEN))
        return;

    if (strlen(gl_hw_info.firmware_type) && gl_hw_info.firmware_type[0] == '2' &&
            gl_hw_info.firmware_type[1] == 'b')
        create_proc_node("firmware_type", gl_hw_info.firmware_type);
    else
        create_proc_node("firmware_type", "2c");
}

void make_factory_data_from_sndata(void)
{
    make_device_mac();
    make_device_ddns();
    make_device_sn();
    make_device_sn_bak();
    make_device_cert();
    make_device_country_code();
    make_device_submodel();
    make_firmware_type_flag();
    make_usb_enable_code();
}

static bool board_prop_skip(const char *name)
{
    if (!strncmp(name, GL_SN_DATA_FACTORY_PREFIX, strlen(GL_SN_DATA_FACTORY_PREFIX)))
        return true;
    if (!strcmp(name, "name") || !strcmp(name, "compatible") ||
            !strcmp(name, "#address-cells") || !strcmp(name, "#size-cells"))
        return true;
    return false;
}

/*
 * 与 main.c probe 中 flash_size + for_each_property_of_node 等价（无 DT 时）：
 *   flash_size=<MiB 整数>  -> proc 为 "<MiB> MiB"
 *   其它 key=value         -> proc（跳过 factory_data.* 工厂行）
 * 值必须堆上分配：不能指向解析用的行缓冲，否则多行时指针会被覆盖。
 */
void gl_hw_info_board_props_from_sndata(void)
{
    const char *p;
    char line[256];
    char *eq;
    char *k;
    char *v;
    char *stored;
    size_t copylen;

    if (ensure_sn2_data_loaded())
        return;

    p = sn2_data_buf;
    while (*p) {
        const char *nl = strchrnul(p, '\n');
        size_t linelen = nl - p;

        copylen = linelen;
        if (copylen >= sizeof(line))
            copylen = sizeof(line) - 1;
        memcpy(line, p, copylen);
        line[copylen] = '\0';
        while (copylen && (line[copylen - 1] == '\r' || line[copylen - 1] == '\n'))
            line[--copylen] = '\0';

        p = *nl ? nl + 1 : nl;

        eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq = '\0';
        k = strim(line);
        v = strim(eq + 1);
        if (!*k)
            continue;

        if (!strcmp(k, "flash_size")) {
            u32 fs = simple_strtoul(v, NULL, 0);

            stored = kasprintf(GFP_KERNEL, "%u MiB", fs);
            if (!stored)
                continue;
            if (!create_proc_node("flash_size", stored))
                kfree(stored);
            continue;
        }

        if (board_prop_skip(k))
            continue;

        if (*v) {
            stored = kstrdup(v, GFP_KERNEL);
            if (!stored)
                continue;
            if (!create_proc_node(k, stored))
                kfree(stored);
        }
    }
}

int gl_hw_info_from_sndata(void)
{
    int err;

    err = ensure_sn2_data_loaded();
    if (err)
        return err;

    pr_debug("gl-hw-info: sn2.data loaded, applying factory and board props\n");
    make_factory_data_from_sndata();
    gl_hw_info_board_props_from_sndata();
    return 0;
}
