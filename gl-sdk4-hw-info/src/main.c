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
    .probe		= gl_hw_info_probe,
    .remove		= gl_hw_info_remove,
    .driver = {
        .name	= GL_HW_INFO_DRV_NAME,
        .of_match_table = gl_hw_info_match,
    }
};

static int __init gl_hw_info_init(void)
{
    int ret;
    ret = proc_init_gl_hw_info();
    if (ret)
        goto err_out;
    ret = platform_driver_register(&gl_hw_info_driver);
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
    platform_driver_unregister(&gl_hw_info_driver);
    proc_remove_gl_hw_info();
}

module_init(gl_hw_info_init);
module_exit(gl_hw_info_exit);

MODULE_AUTHOR("Chongjun Luo <luochongjun@gl-inet.com>");
MODULE_LICENSE("GPL");

