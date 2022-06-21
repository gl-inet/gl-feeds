#include <linux/module.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/i2c.h>

#define ET6326_MAX_LED_NUM			3

#define ET6326_REG_STATUS			0x00
#define ET6326_REG_FLASH_PERIOD		0x01
#define ET6326_REG_FLASH_TON1		0x02
#define ET6326_REG_FLASH_TON2		0x03
#define ET6326_REG_LED_WORK_MODE	0x04
#define ET6326_REG_RAMP_RATE		0x05
#define ET6326_REG_LED1_IOUT		0x06
#define ET6326_REG_LED2_IOUT		0x07
#define ET6326_REG_LED3_IOUT		0x08

#define ET6326_MAX_PERIOD			16380 	/* ms */
#define ET6326_PATTERN_CNT			2

struct et6326_leds_priv;

enum {
	MODE_ALWAYS_OFF,
	MODE_ALWAYS_ON,
	MODE_THREAD
};

struct et6326_led {
	struct et6326_leds_priv *priv;
	struct led_classdev cdev;
	int	mode;
	u8 mode_mask;
	u8 on_shift;
	u8 blink_shift;
	u8 reg_iout;
	char name[0];
};

struct et6326_leds_priv {
	struct et6326_led *leds[ET6326_MAX_LED_NUM];
	struct i2c_client *client;
	struct mutex lock;
};

static int et6326_write_byte(struct i2c_client *client, u8 reg, u8 val)
{
	uint8_t buf[2] = { reg, val };
	struct i2c_msg msg = {
		.addr = client->addr,
		.len = 2,
		.buf = buf
	};
	int ret;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev, "%s: reg 0x%x, val 0x%x, err %d\n",
						__func__, reg, val, ret);
		return ret;
	}
	return 0;
}

static int et6326_read_byte(struct i2c_client *client, u8 reg)
{
	uint8_t buf[] = {reg};
	int ret;

	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = I2C_M_REV_DIR_ADDR,
			.len = 1,
			.buf = buf
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD | I2C_M_NOSTART,
			.len = 1,
			.buf = buf
		}
	};

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: reg 0x%x, err %d\n",
						__func__, reg, ret);
		return ret;
	}

	return buf[0];
}

static int et6326_set_work_mode(struct et6326_led *led, int mode)
{
	struct et6326_leds_priv *priv = led->priv;
	int old_val, new_val;

	old_val = et6326_read_byte(priv->client, ET6326_REG_LED_WORK_MODE);
	if (old_val < 0)
		return old_val;

	new_val = old_val & ~led->mode_mask;

	if (mode == MODE_ALWAYS_ON)
		new_val |= (1 << led->on_shift);
	else if (mode == MODE_THREAD)
		new_val |= 1 << led->blink_shift;

	if (new_val == old_val)
		return 0;

	led->mode = mode;

	return et6326_write_byte(priv->client, ET6326_REG_LED_WORK_MODE, new_val);
}

static int et6326_brightness_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	struct et6326_led *led = container_of(led_cdev, struct et6326_led, cdev);
	struct et6326_leds_priv *priv = led->priv;

	mutex_lock(&priv->lock);

	if (value == LED_OFF)
		et6326_set_work_mode(led, MODE_ALWAYS_OFF);
	else if (led->mode == MODE_ALWAYS_OFF)
		et6326_set_work_mode(led, MODE_ALWAYS_ON);

	et6326_write_byte(priv->client, led->reg_iout, value);

	mutex_unlock(&priv->lock);

	return 0;
}

static void et6326_set_period(struct et6326_leds_priv *priv, u32 period)
{
	et6326_write_byte(priv->client, ET6326_REG_FLASH_PERIOD, period / 128 - 1);
}

static void et6326_set_ton1_percentage(struct et6326_leds_priv *priv, u8 val)
{
	if (val > 100)
		val = 100;

	val = 255 * val / 100;

	et6326_write_byte(priv->client, ET6326_REG_FLASH_TON1, val);
}

static void et6326_set_ramp(struct et6326_leds_priv *priv, u32 ms)
{
	int scaling_reg;
	u8 ramp_reg;

	if (ms > 7680)
		ms = 7680;

	if (ms < 2048) {
		scaling_reg = 0;
		ramp_reg = ms / 128;
	} else if (ms < 4096) {
		scaling_reg = 1;
		ramp_reg = ms / 256;
	} else {
		scaling_reg = 2;
		ramp_reg = ms / 512;
	}

	ramp_reg |= ramp_reg << 4;

	et6326_write_byte(priv->client, ET6326_REG_STATUS, scaling_reg << 5);
	et6326_write_byte(priv->client, ET6326_REG_RAMP_RATE, ramp_reg);
}

static int et6326_pattern_set(struct led_classdev *led_cdev,
				    struct led_pattern *pattern,
				    u32 len, int repeat)
{
	struct et6326_led *led = container_of(led_cdev, struct et6326_led, cdev);
	struct et6326_leds_priv *priv = led->priv;
	int period = pattern[0].delta_t * 2 + pattern[1].delta_t;;

	if (len != ET6326_PATTERN_CNT)
		return -EINVAL;

	mutex_lock(&priv->lock);

	et6326_set_ramp(priv, pattern[0].delta_t);
	et6326_set_ton1_percentage(priv, pattern[1].delta_t * 100 / period);
	et6326_set_period(priv, period);
	et6326_write_byte(priv->client, led->reg_iout, 0xff);
	et6326_set_work_mode(led, MODE_THREAD);

	mutex_unlock(&priv->lock);

	return 0;
}

static int et6326_pattern_clear(struct led_classdev *led_cdev)
{
	struct et6326_led *led = container_of(led_cdev, struct et6326_led, cdev);
	struct et6326_leds_priv *priv = led->priv;

	mutex_lock(&priv->lock);
	et6326_set_work_mode(led, MODE_ALWAYS_OFF);
	mutex_unlock(&priv->lock);

	return 0;
}

static int et6326_blink_set(struct led_classdev *led_cdev,
		unsigned long *delay_on,
		unsigned long *delay_off)
{
	struct et6326_led *led = container_of(led_cdev, struct et6326_led, cdev);
	struct et6326_leds_priv *priv = led->priv;
	unsigned long period;

	/* blink with 1 Hz as default if nothing specified */
	if (!*delay_on && !*delay_off)
		*delay_on = *delay_off = 500;

	if (*delay_on > ET6326_MAX_PERIOD)
		*delay_on = ET6326_MAX_PERIOD;

	if (*delay_off > ET6326_MAX_PERIOD - *delay_on)
		*delay_off = ET6326_MAX_PERIOD - *delay_on;

	period = *delay_on + *delay_off;

	mutex_lock(&priv->lock);

	et6326_set_period(priv, period);
	et6326_set_ton1_percentage(priv, *delay_on * 100 / period);
	et6326_set_ramp(priv, 0);
	et6326_set_work_mode(led, MODE_THREAD);
	et6326_write_byte(priv->client, led->reg_iout, 0xff);

	mutex_unlock(&priv->lock);

	return 0;
}

static int create_led(struct et6326_leds_priv *priv, u32 channel, const char *name)
{
	struct device *dev = &priv->client->dev;
	struct et6326_led *led;
	bool auto_name = true;
	int ret;

	if (priv->leds[channel])
		return -EEXIST;

	if (name && name[0])
		auto_name = false;
	else
		name = "et6326-x";

	led = devm_kzalloc(dev, sizeof(struct et6326_led) + strlen(name) + 1, GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	if (auto_name)
		snprintf(led->name, strlen(name) + 1, "et6326-%u", channel);
	else
		strcpy(led->name, name);

	led->priv = priv;
	led->cdev.name = led->name;
	led->cdev.brightness = LED_OFF;
	led->cdev.brightness_set_blocking = et6326_brightness_set;
	led->cdev.blink_set = et6326_blink_set;
	led->cdev.pattern_set = et6326_pattern_set;
	led->cdev.pattern_clear = et6326_pattern_clear;

	switch (channel) {
	case 0:
		led->mode_mask = 0x03;
		led->on_shift = 0;
		led->blink_shift = 1;
		break;
	case 1:
		led->mode_mask = (0x03 << 2) | (1 << 6);
		led->on_shift = 2;
		led->blink_shift = 3;
		break;
	case 2:
		led->mode_mask = (0x03 << 4) | (1 << 7);
		led->on_shift = 4;
		led->blink_shift = 5;
		break;
	default:
		return -EINVAL;
	}
	
	led->reg_iout = ET6326_REG_LED1_IOUT + channel;

	ret = led_classdev_register(dev, &led->cdev);
	if (ret < 0) {
		dev_err(dev, "couldn't register LED '%s' on channel %d\n", led->name, channel);
		return ret;
	}

	dev_info(&priv->client->dev, "Created a LED '%s' at channel %d\n", led->name, channel);

	priv->leds[channel] = led;

	return 0;
}

static int remove_led(struct et6326_leds_priv *priv, u32 channel)
{
	struct et6326_led *led;

	if (channel > ET6326_MAX_LED_NUM - 1 || channel < 0)
		return -EINVAL;

	led = priv->leds[channel];
	if (!led)
		return -ENODEV;

	et6326_brightness_set(&led->cdev, LED_OFF);

	led_classdev_unregister(&led->cdev);

	dev_info(&priv->client->dev, "LED '%s' was removed\n", led->name);

	priv->leds[channel] = NULL;

	return 0;
}

static ssize_t et6326_export_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t len)
{
	struct et6326_leds_priv *priv = i2c_get_clientdata(to_i2c_client(dev));
	u32 channel;
	int ret;
	char *name;

	name = strchr(buf, ' ');
	if (name) {
		char *ln;

		*name++ = '\0';

		ln = strchr(name, '\n');
		if (ln)
			*ln = '\0';
	}

	ret = sscanf(buf, "%d", &channel);
	if (ret < 1)
		return -EINVAL;

	if (priv->leds[channel])
		return -EBUSY;

	if (channel > ET6326_MAX_LED_NUM - 1 || channel < 0)
		return -EINVAL;

	ret = create_led(priv, channel, name);
	if (ret < 0)
		return ret;

	return len;
}

static ssize_t et6326_unexport_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t len)
{
	struct et6326_leds_priv *priv = i2c_get_clientdata(to_i2c_client(dev));
	u32 channel;
	int ret;

	ret = sscanf(buf, "%u", &channel);
	if (ret < 1)
		return -EINVAL;

	ret = remove_led(priv, channel);
	if (ret)
		return ret;

	return len;
}

static DEVICE_ATTR(export, S_IWUSR, NULL, et6326_export_store);
static DEVICE_ATTR(unexport, S_IWUSR, NULL, et6326_unexport_store);

#ifdef CONFIG_OF
static void of_create_leds(struct et6326_leds_priv *priv, struct device *dev)
{
	struct device_node *child;

	for_each_child_of_node(dev->of_node, child) {
		const char *name;
		u32 channel;

		if (of_property_read_u32(child, "channel", &channel)) {
			dev_err(dev, "%s: invalid channel\n", child->name);
			continue;
		}

		of_property_read_string(child, "label", &name);

		create_led(priv, channel, name);
	}
}
#endif

static int et6326_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct et6326_leds_priv *priv;	
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_NOSTART)) {
		dev_err(&client->dev,
			"need i2c bus that supports protocol mangling\n");
		return -ENODEV;
	}

	priv = devm_kzalloc(dev, sizeof(struct et6326_leds_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->client = client;
	i2c_set_clientdata(client, priv);

	/* Detect ET6326 */
	ret = et6326_write_byte(client, ET6326_REG_STATUS, 0x1f);
	if (ret < 0) {
		dev_err(dev, "failed to detect device\n");
		return ret;
	}

	ret = device_create_file(dev, &dev_attr_export);
	if (ret)
		goto err_attr_export;

	ret = device_create_file(dev, &dev_attr_unexport);
	if (ret)
		goto err_attr_unexport;

	mutex_init(&priv->lock);

#ifdef CONFIG_OF
	of_create_leds(priv, dev);
#endif

	return 0;

err_attr_unexport:
	device_remove_file(&client->dev, &dev_attr_export);
err_attr_export:
	return ret;
}

static int et6326_remove(struct i2c_client *client)
{
	struct et6326_leds_priv *priv = i2c_get_clientdata(client);
	int i;

	for (i = 0; i < ET6326_MAX_LED_NUM; i++)
		remove_led(priv, i);

	et6326_write_byte(client, ET6326_REG_STATUS, 0x1f);

	device_remove_file(&client->dev, &dev_attr_export);
	device_remove_file(&client->dev, &dev_attr_unexport);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id of_et6326_match[] = {
	{ .compatible = "etek,et6326" },
	{},
};

MODULE_DEVICE_TABLE(of, of_et6326_match);
#endif

static const struct i2c_device_id et6326_id[] = {
	{ "et6326", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, et6326_id);

static struct i2c_driver et6326_i2c_driver = {
	.driver	= {
		.name	= "et6326",
		.of_match_table = of_match_ptr(of_et6326_match),
	},
	.probe		= et6326_probe,
	.remove		= et6326_remove,
	.id_table	= et6326_id,
};

module_i2c_driver(et6326_i2c_driver);

MODULE_DESCRIPTION("ET6326 LED driver");
MODULE_AUTHOR("Jianhui Zhao <jianhui.zhao@gl-inet.com>");
MODULE_LICENSE("GPL v2");
