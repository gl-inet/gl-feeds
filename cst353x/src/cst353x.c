// SPDX-License-Identifier: GPL-2.0-or-later
/*
* Driver for I2C connected Hynitron CST353X / CST8xx Touchscreen
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
#include <linux/property.h>
#include <linux/version.h>
#include <linux/workqueue.h>

/* Existing boards use the I2C address to select their wire protocol. */
#define CST353X_FRAME_I2C_ADDR          0x58 /* GL-BE14000, 240x320 */
#define CST353X_REG8_I2C_ADDR           0x15 /* GL-VB7200BE, 172x320 */

/* 32-bit command protocol used by the 240x320 panel. */
#define CST353X_FRAME_CMD               0xD0070000
#define CST353X_END_CMD                 0xD00002AB
#define CST353X_FRAME_DATA_LEN          9

/* Byte-addressed protocol used by the 172x320 panel. */
#define CST353X_REG_DATA                0x00
#define CST353X_REG_CHIP_ID             0xA7
#define CST353X_REG_PROJ_ID             0xA8
#define CST353X_REG8_DATA_LEN           8

#define CST353X_DATA_LEN_MAX            CST353X_FRAME_DATA_LEN
#define CST353X_RAW_X_MAX               239
#define CST353X_RAW_Y_MAX               319

/* CST8xx event in XH[7:6]: 0=down, 1=up, 2=contact. */
#define CST353X_EVENT_DOWN              0x00
#define CST353X_EVENT_UP                0x01
#define CST353X_EVENT_CONTACT           0x02

struct cst353x_priv;

struct cst353x_variant {
    const char *name;
    u16 raw_x_max;
    u16 raw_y_max;
    u16 default_width;
    u16 default_height;
    u16 reset_delay_ms;
    u16 release_delay_ms;
    bool invert_x;
    bool one_based_x;
    int (*identify)(struct cst353x_priv *priv);
    int (*read_touch)(struct cst353x_priv *priv);
    int (*finish_frame)(struct cst353x_priv *priv);
};

struct cst353x_touch_info {
    u8 touch;
    u16 raw_x;
    u16 raw_y;
    u16 abs_x;
    u16 abs_y;
};

struct cst353x_priv {
    struct device *dev;
    struct i2c_client *client;
    struct gpio_desc *reset;
    struct input_dev *input;
    const struct cst353x_variant *variant;
    struct cst353x_touch_info info;
    struct delayed_work release_work;
    u16 abs_x_max;
    u16 abs_y_max;
    u8 rxtx[CST353X_DATA_LEN_MAX];
};

static int cst353x_i2c_read_u32(struct cst353x_priv *priv, u32 command,
                u8 *buf, u16 len)
{
    struct i2c_client *client = priv->client;
    struct i2c_msg xfer[2];
    u8 command_buf[4];
    int rc;

    command_buf[0] = (command >> 24) & 0xff;
    command_buf[1] = (command >> 16) & 0xff;
    command_buf[2] = (command >> 8) & 0xff;
    command_buf[3] = command & 0xff;

    xfer[0].addr = client->addr;
    xfer[0].flags = 0;
    xfer[0].len = sizeof(command_buf);
    xfer[0].buf = command_buf;

    xfer[1].addr = client->addr;
    xfer[1].flags = I2C_M_RD;
    xfer[1].len = len;
    xfer[1].buf = buf;

    rc = i2c_transfer(client->adapter, xfer, ARRAY_SIZE(xfer));
    if (rc != ARRAY_SIZE(xfer)) {
        if (rc >= 0)
            rc = -EIO;
        dev_err(&client->dev, "i2c rx err: %d (cmd=0x%08x)\n",
            rc, command);
        return rc;
    }

    return 0;
}

static int cst353x_i2c_write_u32(struct cst353x_priv *priv, u32 command)
{
    struct i2c_client *client = priv->client;
    struct i2c_msg xfer;
    u8 command_buf[4];
    int rc;

    command_buf[0] = (command >> 24) & 0xff;
    command_buf[1] = (command >> 16) & 0xff;
    command_buf[2] = (command >> 8) & 0xff;
    command_buf[3] = command & 0xff;

    xfer.addr = client->addr;
    xfer.flags = 0;
    xfer.len = sizeof(command_buf);
    xfer.buf = command_buf;

    rc = i2c_transfer(client->adapter, &xfer, 1);
    if (rc != 1) {
        if (rc >= 0)
            rc = -EIO;
        dev_err(&client->dev, "i2c tx err: %d (cmd=0x%08x)\n",
            rc, command);
        return rc;
    }

    return 0;
}

static int cst353x_i2c_read_reg8(struct cst353x_priv *priv, u8 reg,
                u8 *buf, u16 len)
{
    struct i2c_client *client = priv->client;
    struct i2c_msg xfer[2];
    int rc;

    xfer[0].addr = client->addr;
    xfer[0].flags = 0;
    xfer[0].len = 1;
    xfer[0].buf = &reg;

    xfer[1].addr = client->addr;
    xfer[1].flags = I2C_M_RD;
    xfer[1].len = len;
    xfer[1].buf = buf;

    rc = i2c_transfer(client->adapter, xfer, ARRAY_SIZE(xfer));
    if (rc != ARRAY_SIZE(xfer)) {
        if (rc >= 0)
            rc = -EIO;
        dev_err(&client->dev, "i2c rx err: %d (reg=0x%02x)\n",
            rc, reg);
        return rc;
    }

    return 0;
}

static int cst353x_frame_read_touch(struct cst353x_priv *priv)
{
    u8 *raw = priv->rxtx;
    u16 checksum;
    u16 expected;
    int rc;

    rc = cst353x_i2c_read_u32(priv, CST353X_FRAME_CMD, raw,
                CST353X_FRAME_DATA_LEN);
    if (rc)
        return rc;

    checksum = raw[0] | (raw[1] << 8);
    expected = 0x55 + raw[4] + raw[5] + raw[6] + raw[7] + raw[8];
    if (checksum != expected) {
        dev_dbg(priv->dev, "invalid frame checksum: 0x%04x != 0x%04x\n",
            checksum, expected);
        return -EBADMSG;
    }

    priv->info.touch = (raw[8] >> 4) != 0;
    priv->info.raw_x = raw[4] | ((raw[7] & 0x0f) << 8);
    priv->info.raw_y = raw[5] | ((raw[7] & 0xf0) << 4);

    return 0;
}

static int cst353x_frame_finish(struct cst353x_priv *priv)
{
    return cst353x_i2c_write_u32(priv, CST353X_END_CMD);
}

static int cst353x_reg8_identify(struct cst353x_priv *priv)
{
    u8 chip_id;
    u8 proj_id;
    int rc;

    rc = cst353x_i2c_read_reg8(priv, CST353X_REG_CHIP_ID, &chip_id, 1);
    if (rc)
        return rc;

    rc = cst353x_i2c_read_reg8(priv, CST353X_REG_PROJ_ID, &proj_id, 1);
    if (rc)
        return rc;

    dev_info(priv->dev, "chip_id=0x%02x proj_id=0x%02x\n",
        chip_id, proj_id);

    return 0;
}

static int cst353x_reg8_read_touch(struct cst353x_priv *priv)
{
    u8 *raw = priv->rxtx;
    u8 event;
    int rc;

    rc = cst353x_i2c_read_reg8(priv, CST353X_REG_DATA, raw,
                    CST353X_REG8_DATA_LEN);
    if (rc)
        return rc;

    event = (raw[3] >> 6) & 0x03;
    switch (event) {
    case CST353X_EVENT_DOWN:
    case CST353X_EVENT_CONTACT:
        priv->info.touch = 1;
        break;
    case CST353X_EVENT_UP:
        priv->info.touch = 0;
        break;
    default:
        dev_dbg(priv->dev, "unsupported touch event: %u\n", event);
        return -EPROTO;
    }

    priv->info.raw_x = ((raw[3] & 0x0f) << 8) | raw[4];
    priv->info.raw_y = ((raw[5] & 0x0f) << 8) | raw[6];

    dev_dbg(priv->dev, "event=%u raw=%*ph\n", event,
        (int)CST353X_REG8_DATA_LEN, raw);

    return 0;
}

static const struct cst353x_variant cst353x_frame_variant = {
    .name = "frame32",
    .raw_x_max = CST353X_RAW_X_MAX,
    .raw_y_max = CST353X_RAW_Y_MAX,
    .default_width = 240,
    .default_height = 320,
    .reset_delay_ms = 100,
    .release_delay_ms = 40,
    .invert_x = true,
    .one_based_x = true,
    .read_touch = cst353x_frame_read_touch,
    .finish_frame = cst353x_frame_finish,
};

static const struct cst353x_variant cst353x_reg8_variant = {
    .name = "reg8",
    .raw_x_max = CST353X_RAW_X_MAX,
    .raw_y_max = CST353X_RAW_Y_MAX,
    .default_width = 172,
    .default_height = 320,
    .reset_delay_ms = 200,
    .identify = cst353x_reg8_identify,
    .read_touch = cst353x_reg8_read_touch,
};

static const struct cst353x_variant *
cst353x_select_variant(struct i2c_client *client)
{
    const struct cst353x_variant *variant;

    variant = device_get_match_data(&client->dev);
    if (variant)
        return variant;

    switch (client->addr) {
    case CST353X_FRAME_I2C_ADDR:
        return &cst353x_frame_variant;
    case CST353X_REG8_I2C_ADDR:
        return &cst353x_reg8_variant;
    default:
        return ERR_PTR(-ENODEV);
    }
}

static int cst353x_read_dimensions(struct cst353x_priv *priv)
{
    u32 width = priv->variant->default_width;
    u32 height = priv->variant->default_height;

    device_property_read_u32(priv->dev, "touchscreen-size-x", &width);
    device_property_read_u32(priv->dev, "touchscreen-size-y", &height);

    if (!width || width > 65536 || !height || height > 65536) {
        dev_err(priv->dev, "invalid touchscreen size %ux%u\n",
            width, height);
        return -EINVAL;
    }

    priv->abs_x_max = width - 1;
    priv->abs_y_max = height - 1;

    return 0;
}

static void cst353x_map_coordinates(struct cst353x_priv *priv)
{
    const struct cst353x_variant *variant = priv->variant;
    u16 raw_x = priv->info.raw_x;
    u16 raw_y = priv->info.raw_y;

    /* The frame32 protocol reports X in the range 1..240. */
    if (variant->one_based_x && raw_x)
        raw_x--;

    if (raw_x > variant->raw_x_max)
        raw_x = variant->raw_x_max;
    if (raw_y > variant->raw_y_max)
        raw_y = variant->raw_y_max;

    if (variant->invert_x)
        raw_x = variant->raw_x_max - raw_x;

    priv->info.abs_x = DIV_ROUND_CLOSEST((u32)raw_x * priv->abs_x_max,
                        variant->raw_x_max);
    priv->info.abs_y = DIV_ROUND_CLOSEST((u32)raw_y * priv->abs_y_max,
                        variant->raw_y_max);
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
    input_set_abs_params(priv->input, ABS_X, 0, priv->abs_x_max, 0, 0);
    input_set_abs_params(priv->input, ABS_Y, 0, priv->abs_y_max, 0, 0);
    input_set_abs_params(priv->input, ABS_MT_TRACKING_ID, 0, 0, 0, 0);

    return input_register_device(priv->input);
}

static void cst353x_reset(struct cst353x_priv *priv)
{
    /* The reset GPIO is active low; these are logical GPIO values. */
    gpiod_set_value_cansleep(priv->reset, 1);
    msleep(50);
    gpiod_set_value_cansleep(priv->reset, 0);
    msleep(priv->variant->reset_delay_ms);
}

static void cst353x_release_work(struct work_struct *work)
{
    struct cst353x_priv *priv;

    priv = container_of(work, struct cst353x_priv, release_work.work);
    if (!priv->info.touch)
        input_sync(priv->input);
}

static void cst353x_cancel_work(void *data)
{
    struct cst353x_priv *priv = data;

    cancel_delayed_work_sync(&priv->release_work);
}

static void cst353x_report_touch(struct cst353x_priv *priv)
{
    if (priv->info.touch) {
        input_report_abs(priv->input, ABS_X, priv->info.abs_x);
        input_report_abs(priv->input, ABS_Y, priv->info.abs_y);
        input_report_abs(priv->input, ABS_MT_TRACKING_ID, 0);
        input_report_key(priv->input, BTN_TOUCH, 1);
        input_sync(priv->input);
        return;
    }

    input_report_key(priv->input, BTN_TOUCH, 0);
    input_report_abs(priv->input, ABS_MT_TRACKING_ID, -1);
    if (priv->variant->release_delay_ms) {
        mod_delayed_work(system_wq, &priv->release_work,
            msecs_to_jiffies(priv->variant->release_delay_ms));
    } else {
        input_sync(priv->input);
    }
}

static irqreturn_t cst353x_irq_cb(int irq, void *cookie)
{
    struct cst353x_priv *priv = cookie;
    int rc;

    rc = priv->variant->read_touch(priv);
    if (!rc) {
        cst353x_map_coordinates(priv);
        dev_dbg(priv->dev, "x=%u y=%u touch=%u\n",
            priv->info.abs_x, priv->info.abs_y, priv->info.touch);
        cst353x_report_touch(priv);
    }

    /* The frame32 protocol requires this even after a bad frame. */
    if (priv->variant->finish_frame)
        priv->variant->finish_frame(priv);

    return IRQ_HANDLED;
}

static int cst353x_probe_common(struct i2c_client *client)
{
    struct device *dev = &client->dev;
    struct cst353x_priv *priv;
    int rc;

    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->dev = dev;
    priv->client = client;
    priv->variant = cst353x_select_variant(client);
    if (IS_ERR(priv->variant)) {
        rc = PTR_ERR(priv->variant);
        dev_err(dev, "unsupported I2C address 0x%02x\n", client->addr);
        return rc;
    }

    rc = cst353x_read_dimensions(priv);
    if (rc)
        return rc;

    INIT_DELAYED_WORK(&priv->release_work, cst353x_release_work);

    priv->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(priv->reset)) {
        rc = PTR_ERR(priv->reset);
        dev_err(dev, "failed to request reset GPIO: %d\n", rc);
        return rc;
    }

    cst353x_reset(priv);

    if (priv->variant->identify) {
        rc = priv->variant->identify(priv);
        if (rc) {
            dev_err(dev, "chip identification failed: %d\n", rc);
            return rc;
        }
    }

    rc = cst353x_register_input(priv);
    if (rc) {
        dev_err(dev, "input registration failed: %d\n", rc);
        return rc;
    }

    /* Release order: IRQ, delayed work, then the input device. */
    rc = devm_add_action_or_reset(dev, cst353x_cancel_work, priv);
    if (rc)
        return rc;

    i2c_set_clientdata(client, priv);
    rc = devm_request_threaded_irq(dev, client->irq, NULL, cst353x_irq_cb,
                    IRQF_ONESHOT, dev->driver->name, priv);
    if (rc) {
        dev_err(dev, "IRQ request failed: %d\n", rc);
        return rc;
    }

    dev_info(dev, "using %s protocol at 0x%02x, input size %ux%u\n",
        priv->variant->name, client->addr,
        priv->abs_x_max + 1, priv->abs_y_max + 1);

    return 0;
}

/* The upstream I2C probe callback changed to one argument in Linux 6.3. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
static int cst353x_probe(struct i2c_client *client)
{
    return cst353x_probe_common(client);
}
#else
static int cst353x_probe(struct i2c_client *client,
            const struct i2c_device_id *id)
{
    (void)id;
    return cst353x_probe_common(client);
}
#endif

static const struct i2c_device_id cst353x_id[] = {
    { "cst3530", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, cst353x_id);

static const struct of_device_id cst353x_of_match[] = {
    {
        .compatible = "hynitron,cst3530-frame",
        .data = &cst353x_frame_variant,
    },
    {
        .compatible = "hynitron,cst3530-reg8",
        .data = &cst353x_reg8_variant,
    },
    {
        /* Existing DTS files select the variant from the I2C address. */
        .compatible = "hynitron,cst3530",
    },
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
