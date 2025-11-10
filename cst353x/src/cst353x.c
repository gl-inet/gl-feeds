// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Driver for I2C connected Hynitron CST353X Touchscreen
 *
 * Copyright (C) 2024 Oleh Kuzhylnyi <kuzhylol@gmail.com>
 */
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#define BOOT_I2C_ADDR 0x34

enum cst353x_registers {
    CST353X_FRAME = 0xD0070000,
    CST353X_END = 0xD00002AB,
};

struct cst353x_touch_info {
    u8 touch;
    u16 abs_x;
    u16 abs_y;
} __packed;

struct cst353x_priv {
    struct device *dev;
    struct i2c_client *client;
    struct gpio_desc *reset;
    struct input_dev *input;
    struct cst353x_touch_info info;

    struct delayed_work cst_work;

    u8 rxtx[9];
};

static int cst353x_i2c_read_register(struct cst353x_priv *priv, u32 reg)
{
    struct i2c_client *client;
    struct i2c_msg xfer[2];
    u8 reg_buf[4];
    int rc;

    client = priv->client;

    reg_buf[0] = (reg >> 24) & 0xFF;
    reg_buf[1] = (reg >> 16) & 0xFF;
    reg_buf[2] = (reg >> 8) & 0xFF;
    reg_buf[3] = reg & 0xFF;

    xfer[0].addr = client->addr;
    xfer[0].flags = 0;
    xfer[0].len = sizeof(reg_buf);
    xfer[0].buf = reg_buf;

    xfer[1].addr = client->addr;
    xfer[1].flags = I2C_M_RD;
    xfer[1].len = sizeof(priv->rxtx);
    xfer[1].buf = priv->rxtx;

    rc = i2c_transfer(client->adapter, xfer, ARRAY_SIZE(xfer));
    if (rc != ARRAY_SIZE(xfer)) {
        if (rc >= 0)
            rc = -EIO;
    } else {
        rc = 0;
    }

    if (rc < 0)
        dev_err(&client->dev, "i2c rx err: %d\n", rc);

    return rc;
}

static int cst353x_i2c_write_register(struct cst353x_priv *priv, u32 reg)
{
    struct i2c_client *client;
    struct i2c_msg xfer[1];
    u8 reg_buf[4];
    int rc;

    client = priv->client;

    reg_buf[0] = (reg >> 24) & 0xFF;
    reg_buf[1] = (reg >> 16) & 0xFF;
    reg_buf[2] = (reg >> 8) & 0xFF;
    reg_buf[3] = reg & 0xFF;

    xfer[0].addr = client->addr;
    xfer[0].flags = 0;
    xfer[0].len = sizeof(reg_buf);
    xfer[0].buf = reg_buf;

    rc = i2c_transfer(client->adapter, xfer, ARRAY_SIZE(xfer));
    if (rc != ARRAY_SIZE(xfer)) {
        if (rc >= 0)
            rc = -EIO;
    } else {
        rc = 0;
    }

    if (rc < 0)
        dev_err(&client->dev, "i2c tx data err: %d\n", rc);

    return rc;
}



static int cst353x_process_touch(struct cst353x_priv *priv)
{
    u8 *raw;
    int rc;

    rc = cst353x_i2c_read_register(priv, CST353X_FRAME);
    if (!rc) {
        raw = priv->rxtx;


        if ((raw[0] + (raw[1] << 8)) != (0x55 + raw[4] + raw[5] + raw[6] + raw[7] + raw[8])) {
            return -1;
        }

        priv->info.touch = (raw[8] >> 4) == 0x0 ? 0 : 1;
        priv->info.abs_x = 240 - (raw[4] + ((raw[7] & 0x0F) << 8));
        priv->info.abs_y = raw[5] + ((raw[7] & 0xF0) << 4);

        //dev_info(priv->dev, "x: %d, y: %d, t: %d\n",
        //         priv->info.abs_x, priv->info.abs_y, priv->info.touch);
    }

    return rc;
}

static int cst353x_register_input(struct cst353x_priv *priv)
{
    priv->input = devm_input_allocate_device(priv->dev);
    if (!priv->input)
        return -ENOMEM;

    priv->input->name = "Hynitron CST353X Touchscreen";
    priv->input->phys = "input/ts";
    priv->input->id.bustype = BUS_I2C;
    input_set_drvdata(priv->input, priv);

    input_set_capability(priv->input, EV_KEY, BTN_TOUCH);
    input_set_abs_params(priv->input, ABS_X, 0, 239, 0, 0);
    input_set_abs_params(priv->input, ABS_Y, 0, 319, 0, 0);
    input_set_abs_params(priv->input, ABS_MT_TRACKING_ID, 0, 0, 0, 0);

    return input_register_device(priv->input);
}

static void cst353x_reset(struct cst353x_priv *priv)
{
    gpiod_set_value_cansleep(priv->reset, 1);
    msleep(50);
    gpiod_set_value_cansleep(priv->reset, 0);
    msleep(100);
}

static irqreturn_t cst353x_irq_cb(int irq, void *cookie)
{
    struct cst353x_priv *priv = (struct cst353x_priv *)cookie;

    if (!cst353x_process_touch(priv)) {
        if (priv->info.touch) {
            input_report_abs(priv->input, ABS_X, priv->info.abs_x);
            input_report_abs(priv->input, ABS_Y, priv->info.abs_y);
            input_report_abs(priv->input, ABS_MT_TRACKING_ID, 0);
            input_report_key(priv->input, BTN_TOUCH, 1);
            input_sync(priv->input);
        } else {
            input_report_abs(priv->input, ABS_MT_TRACKING_ID, -1);
            mod_delayed_work(system_wq, &priv->cst_work, msecs_to_jiffies(40));
        }
    }

	cst353x_i2c_write_register(priv, CST353X_END);
    return IRQ_HANDLED;
}

static void work_callback(struct work_struct *work)
{
    struct cst353x_priv *priv = container_of(work, struct cst353x_priv, cst_work.work);
    if (priv) {
        if (priv->info.touch == 0)
            input_sync(priv->input);
    }
}

static int cst353x_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct cst353x_priv *priv;
    struct device *dev = &client->dev;
    int rc;

    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->dev = dev;
    priv->client = client;

    INIT_DELAYED_WORK(&priv->cst_work, work_callback);

    priv->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(priv->reset)) {
        int error = PTR_ERR(priv->reset);
        dev_err(dev, "Failed to request RESET gpio: %d\n", error);
        return error;
    }

    cst353x_reset(priv);

    rc = cst353x_register_input(priv);
    if (rc) {
        dev_err(dev, "input register failed\n");
        return rc;
    }

    rc = devm_request_threaded_irq(dev, client->irq, NULL, cst353x_irq_cb,
                                   IRQF_ONESHOT, dev->driver->name, priv);
    if (rc) {
        dev_err(dev, "irq request failed\n");
        return rc;
    }

    printk("[ltdbg] [%s %d]\n", __FUNCTION__, __LINE__);
    return 0;
}

static const struct i2c_device_id cst353x_id[] = {
    { .name = "cst3530", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, cst353x_id);

static const struct of_device_id cst353x_of_match[] = {
    { .compatible = "hynitron,cst3530", },
    { }
};
MODULE_DEVICE_TABLE(of, cst353x_of_match);

static struct i2c_driver cst353x_driver = {
    .driver = {
        .name = "cst353x",
        .of_match_table = cst353x_of_match,
    },
    .id_table = cst353x_id,
    .probe = cst353x_probe,
};

module_i2c_driver(cst353x_driver);

MODULE_DESCRIPTION("Hynitron CST353X Touchscreen Driver");
MODULE_LICENSE("GPL");
