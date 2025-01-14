/*
 *  GL hardware information driver
 *
 *  Copyright (C) 2021 Chonejun Luo <luochongjun@gl-inet.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/module.h>
#include <linux/sizes.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/etherdevice.h>
#include <linux/mtd/mtd.h>
#include "gl-hw-info.h"
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>

struct glinet_hw_info gl_hw_info = {};

static int gl_hw_info_probe(struct platform_device *pdev)
{
#ifdef CONFIG_OF
    struct property *pp = NULL;
    struct device_node *np = pdev->dev.of_node;
    struct device_node *data_np = of_find_node_by_name(NULL, "factory_data");
    static char flash_size_str[64] = "";
    int flash_size = 0;

    if (data_np) {
        make_factory_data(data_np);
        of_node_put(data_np);
    }

    of_property_read_u32(np, "flash_size", &flash_size);
    sprintf(flash_size_str, "%d MiB", flash_size);
    create_proc_node("flash_size", flash_size_str);

    for_each_property_of_node(np, pp) {
        const char *value = NULL;

        if (!strcmp(pp->name, "name") || !strcmp(pp->name, "compatible") ||
                !strcmp(pp->name, "#address-cells") || !strcmp(pp->name, "#size-cells") ||
                !strcmp(pp->name, "flash_size"))
            continue;

        if (!pp->value)
            continue;

        if (strnlen(pp->value, pp->length) < pp->length)
            value = (char *)pp->value;

        create_proc_node(pp->name, value ? value : "True");
    }
#endif
    printk("install gl_hw_info\n");

    return 0;
}

static int gl_hw_info_remove(struct platform_device *pdev)
{
    printk("remove gl_hw_info\n");
    return 0;
}

static const struct of_device_id gl_hw_info_match[] = {
    { .compatible = "gl-hw-info" },
    {}
};

static struct platform_driver gl_hw_info_driver = {
    .probe      = gl_hw_info_probe,
    .remove     = gl_hw_info_remove,
    .driver = {
        .name    = GL_HW_INFO_DRV_NAME,
        .of_match_table = gl_hw_info_match,
    }
};

static int gl_find_hw_info(void)
{
    struct file *           file;
    loff_t                  offset      = 0;
    ssize_t                 bytes_read;
    char *                  buffer;
    char *                  token;
    char *                  line;
    int                     ret         = 0;

    buffer = kmalloc(GL_DEVICE_BUFFER_SIZE, GFP_KERNEL);
    if (!buffer) {
        ret = -1;
        goto out;
    }

    if (IS_ERR(file = filp_open(GL_DEVICE_FILE_PATH, O_RDONLY, 0))) {
        ret = -1;
        goto free;
    }

    bytes_read = kernel_read(file, buffer, GL_DEVICE_BUFFER_SIZE - 1, &offset);
    if (bytes_read < 0) {
        ret = -1;
        goto close;
    }
    buffer[bytes_read] = '\0';

    line = buffer;
    while ((line = strsep(&buffer, "\n")) != NULL) {
        if (strncmp(line, GL_DEVICE_MAC, sizeof(GL_DEVICE_MAC)-1) == 0) {
            token = line + sizeof(GL_DEVICE_MAC)-1;
            create_proc_node("device_mac", token);
        } else if (strncmp(line, GL_DEVICE_SN, sizeof(GL_DEVICE_SN)-1) == 0) {
            token = line + sizeof(GL_DEVICE_SN)-1;
            create_proc_node("device_sn", token);
        } else if (strncmp(line, GL_DEVICE_DDNS, sizeof(GL_DEVICE_DDNS)-1) == 0) {
            token = line + sizeof(GL_DEVICE_DDNS)-1;
            create_proc_node("device_ddns", token);
        } else if (strncmp(line, GL_DEVICE_MODEL, sizeof(GL_DEVICE_MODEL)-1) == 0) {
            token = line + sizeof(GL_DEVICE_MODEL)-1;
            create_proc_node("model", token);
        } else if (strncmp(line, GL_DEVICE_COUNTRY_CODE, sizeof(GL_DEVICE_COUNTRY_CODE)-1) == 0) {
            token = line + sizeof(GL_DEVICE_COUNTRY_CODE)-1;
            create_proc_node("country_code", token);
        } else if (strncmp(line, GL_DEVICE_LAN, sizeof(GL_DEVICE_LAN)-1) == 0) {
            token = line + sizeof(GL_DEVICE_LAN)-1;
            create_proc_node("lan", token);
        } else if (strncmp(line, GL_DEVICE_WAN, sizeof(GL_DEVICE_WAN)-1) == 0) {
            token = line + sizeof(GL_DEVICE_WAN)-1;
            create_proc_node("wan", token);
        }
    }

close:
    filp_close(file, NULL);
free:
    kfree(buffer);
out:
    return ret;
}

static int __init gl_hw_info_init(void)
{
    int ret;
    struct device_node *data_np = of_find_node_by_name(NULL, "gl-hw");

    ret = proc_init_gl_hw_info();
    if (ret)
        goto err_out;

    if (data_np)
        ret = platform_driver_register(&gl_hw_info_driver);
    else
        ret = gl_find_hw_info();

    if (ret)
        goto err_proc_exit;

    return ret;
err_proc_exit:
    proc_remove_gl_hw_info();
err_out:
    return ret;
}

static void __exit gl_hw_info_exit(void)
{
    struct device_node *data_np = of_find_node_by_name(NULL, "gl-hw");

    if (data_np)
        platform_driver_unregister(&gl_hw_info_driver);

    proc_remove_gl_hw_info();
}

module_init(gl_hw_info_init);
module_exit(gl_hw_info_exit);

MODULE_AUTHOR("Chongjun Luo <luochongjun@gl-inet.com>");
MODULE_LICENSE("GPL");

