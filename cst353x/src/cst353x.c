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
	CST353X_FRAME = 0x00,
	CST353X_MOTION = 0xEC,
};

enum cst353x_gestures {
	CST353X_SWIPE_UP = 0x01,
	CST353X_SWIPE_DOWN = 0x02,
	CST353X_SWIPE_LEFT = 0x03,
	CST353X_SWIPE_RIGHT = 0x04,
	CST353X_SINGLE_TAP = 0x05,
	CST353X_LONG_PRESS = 0x0C,
};

struct cst353x_touch_info {
	u8 gesture;
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

	u8 rxtx[8];
};

struct cst353x_event_mapping {
	enum cst353x_gestures gesture;
	u16 event_code;
};

static const struct cst353x_event_mapping cst353x_event_map[] = {
	{CST353X_SWIPE_UP, BTN_FORWARD},
	{CST353X_SWIPE_DOWN, BTN_BACK},
	{CST353X_SWIPE_LEFT, BTN_LEFT},
	{CST353X_SWIPE_RIGHT, BTN_RIGHT},
	{CST353X_SINGLE_TAP, BTN_TOUCH},
	{CST353X_LONG_PRESS, BTN_TOOL_TRIPLETAP}
};

static int cst353x_i2c_read_register(struct cst353x_priv *priv, u8 reg)
{
	struct i2c_client *client;
	struct i2c_msg xfer[2];
	int rc;

	client = priv->client;

	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = sizeof(reg);
	xfer[0].buf = &reg;

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

static int cst353x_i2c_write_data(struct i2c_client *client, u_int8_t device_addr, u_int16_t reg_addr, u_int8_t *data, u_int16_t len)
{
	struct i2c_msg xfer[1];
	int rc;

	u_int8_t *buf = (u_int8_t *)kmalloc(len + 2, GFP_KERNEL);
	if (!buf) {
		dev_err(&client->dev, "kmalloc faile\n");
		return -1;
	}

	buf[0] = reg_addr >> 8;
	buf[1] = reg_addr;
	memcpy(buf + 2, data, len);

	xfer[0].addr = device_addr;
	xfer[0].flags = 0;
	xfer[0].len = len + 2;
	xfer[0].buf = buf;

	rc = i2c_transfer(client->adapter, xfer, ARRAY_SIZE(xfer));
	kfree(buf);
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

static int cst353x_i2c_read_data(struct i2c_client *client, u_int8_t device_addr, u_int16_t reg_addr, u_int8_t *data, u_int16_t len)
{
	u8 reg[2];
	struct i2c_msg xfer[2];
	int rc;

	reg[0] = reg_addr >> 8;
	reg[1] = reg_addr;

	xfer[0].addr = device_addr;
	xfer[0].flags = 0;
	xfer[0].len = 2;
	xfer[0].buf = reg;

	xfer[1].addr = device_addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = data;

	rc = i2c_transfer(client->adapter, xfer, ARRAY_SIZE(xfer));
	if (rc != ARRAY_SIZE(xfer)) {
		if (rc >= 0)
			rc = -EIO;
	} else {
		rc = 0;
	}

	if (rc < 0)
		dev_err(&client->dev, "i2c rx data err: %d\n", rc);

	return rc;
}

static int cst353x_process_touch(struct cst353x_priv *priv)
{
	u8 *raw;
	int rc;

	rc = cst353x_i2c_read_register(priv, CST353X_FRAME);
	if (!rc) {
		raw = priv->rxtx;

		if ((raw[3] & 0xF0) != 0x40 && (raw[3] & 0xF0) != 0x80 && (raw[3] & 0xF0) != 0x0)
			return -1;

		priv->info.gesture = raw[0];
		priv->info.touch = (raw[3] & 0xF0) == 0x40 ? 0 : 1;
		priv->info.abs_x = ((raw[3] & 0x0F) << 8) | raw[4];
		priv->info.abs_y = ((raw[5] & 0x0F) << 8) | raw[6];

		dev_info(priv->dev, "reg3: 0x%x, reg4: 0x%x, reg5: 0x%x, reg6: 0x%x, x: %d, y: %d, t: %d, g: 0x%x\n",
			raw[3],raw[5],raw[5],raw[6],priv->info.abs_x, priv->info.abs_y, priv->info.touch,
			priv->info.gesture);
	}

	return rc;
}

static int cst353x_register_input(struct cst353x_priv *priv)
{
	unsigned int i;
	priv->input = devm_input_allocate_device(priv->dev);
	if (!priv->input)
		return -ENOMEM;

	priv->input->name = "Hynitron CST353X Touchscreen";
	priv->input->phys = "input/ts";
	priv->input->id.bustype = BUS_I2C;
	input_set_drvdata(priv->input, priv);

	for (i = 0; i < ARRAY_SIZE(cst353x_event_map); i++) {
		input_set_capability(priv->input, EV_KEY,
				     cst353x_event_map[i].event_code);
	}

	input_set_abs_params(priv->input, ABS_X, 0, 240, 0, 0);
	input_set_abs_params(priv->input, ABS_Y, 0, 240, 0, 0);
	input_set_abs_params(priv->input, ABS_MT_TRACKING_ID, 0, 1, 0, 0);

	return input_register_device(priv->input);
}

static void cst353x_reset(struct cst353x_priv *priv)
{
	gpiod_set_value_cansleep(priv->reset, 1);
	msleep(50);
	gpiod_set_value_cansleep(priv->reset, 0);
	msleep(100);
}

static void report_gesture_event(const struct cst353x_priv *priv,
				 enum cst353x_gestures gesture, bool touch)
{
	unsigned int i;
	for (i = 0; i < ARRAY_SIZE(cst353x_event_map); i++) {
		if (cst353x_event_map[i].gesture == gesture) {
			input_report_key(priv->input,
					 cst353x_event_map[i].event_code,
					 touch);
			break;
		}
	}

	if (!touch)
		input_report_key(priv->input, BTN_TOUCH, 0);
}

/*
 * Supports five gestures: TOUCH, LEFT, RIGHT, FORWARD, BACK, and LONG_PRESS.
 * Reports surface interaction, sliding coordinates and finger detachment.
 *
 * 1. TOUCH Gesture Scenario:
 *
 * [x/y] [touch] [gesture] [Action] [Report ABS] [Report Key]
 *  x y   true    0x00      Touch    ABS_X_Y      BTN_TOUCH
 *  x y   true    0x00      Slide    ABS_X_Y
 *  x y   false   0x05      Gesture               BTN_TOUCH
 *
 * 2. LEFT, RIGHT, FORWARD, BACK, and LONG_PRESS Gestures Scenario:
 *
 * [x/y] [touch] [gesture] [Action] [Report ABS] [Report Key]
 *  x y   true    0x00      Touch    ABS_X_Y      BTN_TOUCH
 *  x y   true    0x01      Gesture  ABS_X_Y      BTN_FORWARD
 *  x y   true    0x01      Slide    ABS_X_Y
 *  x y   false   0x01      Detach                BTN_FORWARD | BTN_TOUCH
 */
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

		//if (priv->info.gesture)
		//	report_gesture_event(priv, priv->info.gesture,
		//			     priv->info.touch);

		//input_sync(priv->input);
	}

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

ssize_t read_binary_file(const char *path, char **buffer) {
	struct file *file;
	loff_t file_size;
	ssize_t bytes_read;
	file = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(file)) {
		return PTR_ERR(file);
	}

	file_size = i_size_read(file->f_path.dentry->d_inode);
	*buffer = kmalloc(file_size, GFP_KERNEL);
	if (!*buffer) {
		filp_close(file, NULL);
		return -ENOMEM;
	}

	bytes_read = kernel_read(file, *buffer, file_size, &file->f_pos);
	filp_close(file, NULL);

	if (bytes_read < 0) {
		kfree(*buffer);
		return bytes_read;
	}

	return bytes_read;
}


static u_int8_t cst353x_enter_bootmode(struct cst353x_priv *priv) {
	char retrycnt = 10;

	gpiod_set_value_cansleep(priv->reset, 1);
	mdelay(10);
	gpiod_set_value_cansleep(priv->reset, 0);
	mdelay(10);

	while(retrycnt--) {
		u_int8_t data = 0xAB;
		cst353x_i2c_write_data(priv->client, BOOT_I2C_ADDR, 0xA001, &data, 1);
		msleep(2);
		cst353x_i2c_read_data(priv->client, BOOT_I2C_ADDR, 0xA003, &data, 1);
		msleep(2);
		if (data == 0xc1)
			return 0;
	}

	dev_err(priv->dev, "enter bootmode fail\n");
	return -1;
}

static u_int16_t cst353x_read_firmware_checksum(struct cst353x_priv *priv)
{
	u_int8_t checksum[2];
	u_int16_t ret = -1;

	if (cst353x_enter_bootmode(priv) == 0) {
		u_int8_t cmd = 0;
		cst353x_i2c_write_data(priv->client, BOOT_I2C_ADDR, 0xA003, &cmd, 1);
		msleep(500);
		cst353x_i2c_read_data(priv->client, BOOT_I2C_ADDR, 0xA008, checksum, 2);
		ret = (checksum[1] << 8) + checksum[0];
	}
	return ret;
}

static int cst353x_update_firmware(struct cst353x_priv *priv, u_int16_t startaddr, u_int16_t length, char *data)
{
	u_int8_t cmd[10];
	u_int32_t i = 0;
	u_int32_t sum_len = 0;
	u_int32_t k_data = length / 512;

	if (cst353x_enter_bootmode(priv) == -1) {
		return -1;
	}

	for(i=0; i<k_data; i++) {
		if (sum_len >= length)
			return -1;

		cmd[0] = startaddr & 0xFF;
		cmd[1] = startaddr >> 8;
		cst353x_i2c_write_data(priv->client, BOOT_I2C_ADDR, 0xA014, cmd, 2);
		msleep(2);

		cst353x_i2c_write_data(priv->client, BOOT_I2C_ADDR, 0xA018, data + i * 512, 512);
		msleep(4);

		cmd[0] = 0xEE;
		cst353x_i2c_write_data(priv->client, BOOT_I2C_ADDR, 0xA004, cmd, 1);
		msleep(100);

		{
			u_int8_t retry_count = 50;
			while(retry_count--) {
				cmd[0] = 0;
				cst353x_i2c_write_data(priv->client, BOOT_I2C_ADDR, 0xA005, cmd, 1);
				if (cmd[0] == 0x55) {
					cmd[0] = 0;
					break;
				}
				msleep(10);
			}
		}

		startaddr += 512;
		sum_len +=512;
	}
	return 0;
}

static void cst353x_try_to_update_tp_firmware(struct cst353x_priv *priv) {
	char *buffer = NULL;
	ssize_t bytes_read;
	u_int16_t startaddr, length, checksum;
	bytes_read = read_binary_file("/etc/gl_screen/firmware/cst3530_firmware.bin", &buffer);
	if (bytes_read < 1)
		goto END;
	else if (bytes_read < 6) {
		dev_err(priv->dev, "firmware size is too small\n");
		goto END;
	}

	startaddr = (buffer[1] << 8) + buffer[0];
	length = (buffer[3] << 8) + buffer[2];
	checksum = (buffer[5] << 8) + buffer[4];

	if (bytes_read < length + 6) {
		dev_err(priv->dev, "firmware format error\n");
		goto END;
	}

	if (cst353x_enter_bootmode(priv) == -1)
		goto END;

	if (cst353x_read_firmware_checksum(priv) != checksum) {
		cst353x_update_firmware(priv, startaddr, length, buffer + 6);
		if (cst353x_read_firmware_checksum(priv) == checksum)
			dev_err(priv->dev, "update firmware success, checksum: %d\n", checksum);
		else
			dev_err(priv->dev, "update firmware fail\n");
	}

END:
	if (buffer)
		kfree(buffer);
	return;
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

	cst353x_try_to_update_tp_firmware(priv);

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
