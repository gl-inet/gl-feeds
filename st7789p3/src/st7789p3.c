/*
 * FB driver for the ST7789P3 LCD Controller
 *
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/module.h>
#include <video/mipi_display.h>
#include <linux/backlight.h>

#include <../drivers/staging/fbtft/fbtft.h>

#define DRVNAME "st7789p3"

#define DEFAULT_GAMMA \
	"70 2C 2E 15 10 09 48 33 53 0B 19 18 20 25\n" \
	"70 2C 2E 15 10 09 48 33 53 0B 19 18 20 25"

#define HSD20_IPS_GAMMA \
	"D0 05 0A 09 08 05 2E 44 45 0F 17 16 2B 33\n" \
	"D0 05 0A 09 08 05 2E 43 45 0F 16 16 2B 33"

#define HSD20_IPS 1

#define MADCTL_BGR BIT(3) /* bitmask for RGB/BGR order */
#define MADCTL_MV BIT(5) /* bitmask for page/column order */
#define MADCTL_MX BIT(6) /* bitmask for column address order */
#define MADCTL_MY BIT(7) /* bitmask for page address order */

/* 82Hz for 12.2ms, configured as 2*12.2ms */
#define PANEL_TE_TIMEOUT_MS  25

// static irqreturn_t panel_te_handler(int irq, void *data)
// {
// 	struct fbtft_par *par = (struct fbtft_par *)data;

// 	complete(&par->panel_te);
// 	return IRQ_HANDLED;
// }

/*
 * init_tearing_effect_line() - init tearing effect line.
 * @par: FBTFT parameter object.
 *
 * Return: 0 on success, or a negative error code otherwise.
 */
// static int init_tearing_effect_line(struct fbtft_par *par)
// {
// 	struct device *dev = par->info->device;
// 	struct gpio_desc *te;
// 	int rc, irq;

// 	te = gpiod_get_optional(dev, "te", GPIOD_IN);
// 	if (IS_ERR(te)) {
// 		dev_err(dev, "Failed to request te GPIO\n");
// 		return PTR_ERR(te);
// 	}
// 	/* if te is NULL, indicating no configuration, directly return success */
// 	if (!te)
// 		return 0;

// 	irq = gpiod_to_irq(te);

// 	/* GPIO is locked as an IRQ, we may drop the reference */
// 	gpiod_put(te);

// 	if (irq < 0)
// 		return irq;

// 	par->irq_te = irq;
// 	init_completion(&par->panel_te);

// 	/* The effective state is high and lasts no more than 1000 microseconds */
// 	rc = devm_request_irq(dev, par->irq_te, panel_te_handler,
// 			      IRQF_TRIGGER_RISING, "TE_GPIO", par);
// 	if (rc) {
// 		dev_err(dev, "TE IRQ request failed.\n");
// 		return rc;
// 	}

// 	disable_irq_nosync(par->irq_te);

// 	return 0;
// }

/**
 * init_display() - initialize the display controller
 *
 * @par: FBTFT parameter object
 *
 * Most of the commands in this init function set their parameters to the
 * same default values which are already in place after the display has been
 * powered up. (The main exception to this rule is the pixel format which
 * would default to 18 instead of 16 bit per pixel.)
 * Nonetheless, this sequence can be used as a template for concrete
 * displays which usually need some adjustments.
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int init_display(struct fbtft_par *par)
{
	par->fbtftops.register_backlight(par);
	//init_tearing_effect_line(par);

	if(par->info->bl_dev) {
		par->info->bl_dev->props.power = FB_BLANK_NORMAL;
		backlight_update_status(par->info->bl_dev);
	}
	par->fbtftops.reset(par);

	write_reg(par, 0x11);

	mdelay(120);
	//write_reg(par, 0xB2, 0x0C, 0x0C, 0x00, 0x33, 0x33);
	//write_reg(par, 0xB0, 0x00, 0xE0);
	write_reg(par, 0x36, 0x00);
	write_reg(par, 0x3A, 0x05);
	write_reg(par, 0xB2, 0x05, 0x05, 0x00, 0x33, 0x33);
	write_reg(par, 0xB7, 0x35);
	write_reg(par, 0xBB, 0x21);
	write_reg(par, 0xc0, 0x2c);
	write_reg(par, 0xc2, 0x01);
	write_reg(par, 0xc3, 0x0b);
	write_reg(par, 0xc4, 0x20);
	write_reg(par, 0xc6, 0x0f);
	write_reg(par, 0xd0, 0xa7, 0xa1);
	write_reg(par, 0xd0, 0xa4, 0xa1);
	//write_reg(par, 0x35, 0x00);
	//write_reg(par, 0x44, 0x00, 0x20);
	write_reg(par, 0xd6, 0xa1);
	write_reg(par, 0xe0, 0xd0, 0x04, 0x08, 0x0a, 0x09, 0x05, 0x2d, 0x43, 0x49, 0x09, 0x16, 0x15, 0x26, 0x2b);
	write_reg(par, 0xe1, 0xd0, 0x03, 0x09, 0x0a, 0x0a, 0x06, 0x2e, 0x44, 0x40, 0x3a, 0x15, 0x15, 0x26, 0x2A);
	write_reg(par, 0x21);

	mdelay(10);
	write_reg(par, 0x29);
	mdelay(120);
	if(par->info->bl_dev) {
		par->info->bl_dev->props.brightness = 5;
		backlight_update_status(par->info->bl_dev);
	}
	return 0;
}

/*
 * write_vmem() - write data to display.
 * @par: FBTFT parameter object.
 * @offset: offset from screen_buffer.
 * @len: the length of data to be writte.
 *
 * Return: 0 on success, or a negative error code otherwise.
 */
static int write_vmem(struct fbtft_par *par, size_t offset, size_t len)
{
	struct device *dev = par->info->device;
	int ret;
	// if (par->irq_te) {
	// 	reinit_completion(&par->panel_te);
	// 	enable_irq(par->irq_te);
	// 	ret = wait_for_completion_timeout(&par->panel_te,
	// 					  msecs_to_jiffies(PANEL_TE_TIMEOUT_MS));
	// 	//if (ret == 0)
	// 	//	dev_err(dev, "wait panel TE timeout\n");

	// 	disable_irq(par->irq_te);
	// }

	ret = 0;
	mutex_lock(&par->gamma.lock);
	switch (par->pdata->display.buswidth) {
	case 8:
		ret = fbtft_write_vmem16_bus8(par, offset, len);
		break;
	case 9:
		ret = fbtft_write_vmem16_bus9(par, offset, len);
		break;
	case 16:
		ret = fbtft_write_vmem16_bus16(par, offset, len);
		break;
	default:
		dev_err(dev, "Unsupported buswidth %d\n",
			par->pdata->display.buswidth);
		break;
	}
	mutex_unlock(&par->gamma.lock);
	return ret;
}

/**
 * set_addr_win() - apply LCD window
 *
 * @par: FBTFT parameter object
 *
 */

static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	// ys += 18;
	// ye += 18;
	// xs += 82;
	// xe += 82;
	write_reg(par, MIPI_DCS_SET_COLUMN_ADDRESS,
		  xs >> 8, xs & 0xFF, xe >> 8, xe & 0xFF);

	write_reg(par, MIPI_DCS_SET_PAGE_ADDRESS,
		  ys >> 8, ys & 0xFF, ye >> 8, ye & 0xFF);

	write_reg(par, MIPI_DCS_WRITE_MEMORY_START);
}

/**
 * set_var() - apply LCD properties like rotation and BGR mode
 *
 * @par: FBTFT parameter object
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int set_var(struct fbtft_par *par)
{
	u8 madctl_par = 0;
	if (par->bgr)
		madctl_par |= MADCTL_BGR;
	switch (par->info->var.rotate) {
	case 0:
		break;
	case 90:
		madctl_par |= (MADCTL_MV | MADCTL_MY);
		break;
	case 180:
		madctl_par |= (MADCTL_MX | MADCTL_MY);
		break;
	case 270:
		madctl_par |= (MADCTL_MV | MADCTL_MX);
		break;
	default:
		return -EINVAL;
	}
	write_reg(par, MIPI_DCS_SET_ADDRESS_MODE, madctl_par);
	return 0;
}

/**
 * set_gamma() - set gamma curves
 *
 * @par: FBTFT parameter object
 * @curves: gamma curves
 *
 * Before the gamma curves are applied, they are preprocessed with a bitmask
 * to ensure syntactically correct input for the display controller.
 * This implies that the curves input parameter might be changed by this
 * function and that illegal gamma values are auto-corrected and not
 * reported as errors.
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int set_gamma(struct fbtft_par *par, u32 *curves)
{
	int i;
	int j;
	int c; /* curve index offset */
	/*
	 * Bitmasks for gamma curve command parameters.
	 * The masks are the same for both positive and negative voltage
	 * gamma curves.
	 */
	static const u8 gamma_par_mask[] = {
		0xFF, /* V63[3:0], V0[3:0]*/
		0x3F, /* V1[5:0] */
		0x3F, /* V2[5:0] */
		0x1F, /* V4[4:0] */
		0x1F, /* V6[4:0] */
		0x3F, /* J0[1:0], V13[3:0] */
		0x7F, /* V20[6:0] */
		0x77, /* V36[2:0], V27[2:0] */
		0x7F, /* V43[6:0] */
		0x3F, /* J1[1:0], V50[3:0] */
		0x1F, /* V57[4:0] */
		0x1F, /* V59[4:0] */
		0x3F, /* V61[5:0] */
		0x3F, /* V62[5:0] */
	};

	for (i = 0; i < par->gamma.num_curves; i++) {
		c = i * par->gamma.num_values;
		for (j = 0; j < par->gamma.num_values; j++)
			curves[c + j] &= gamma_par_mask[j];
		write_reg(par, 0xe0 + i,
			  curves[c + 0],  curves[c + 1],  curves[c + 2],
			  curves[c + 3],  curves[c + 4],  curves[c + 5],
			  curves[c + 6],  curves[c + 7],  curves[c + 8],
			  curves[c + 9],  curves[c + 10], curves[c + 11],
			  curves[c + 12], curves[c + 13]);
	}
	return 0;
}

/**
 * blank() - blank the display
 *
 * @par: FBTFT parameter object
 * @on: whether to enable or disable blanking the display
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int blank(struct fbtft_par *par, bool on)
{
	mutex_lock(&par->gamma.lock);
	if (on) {
		if (par->info->bl_dev) {
			par->info->bl_dev->props.power = FB_BLANK_NORMAL;
			backlight_update_status(par->info->bl_dev);
		}
		write_reg(par, MIPI_DCS_SET_DISPLAY_OFF);
		//write_reg(par, MIPI_DCS_ENTER_SLEEP_MODE);
		mdelay(120);
	} else {
		//write_reg(par, MIPI_DCS_EXIT_SLEEP_MODE);
		//mdelay(120);
		write_reg(par, MIPI_DCS_SET_DISPLAY_ON);
		mdelay(20);
		write_reg(par, MIPI_DCS_SET_DISPLAY_ON);
		mdelay(20);
		if (par->info->bl_dev) {
			par->info->bl_dev->props.power = FB_BLANK_UNBLANK;
			backlight_update_status(par->info->bl_dev);
		}
	}
	mutex_unlock(&par->gamma.lock);
	return 0;
}

/**
 * register_chip_backlight() - register chip backlight
 *
 * @par: FBTFT parameter object
 *
 */

static void register_chip_backlight(struct fbtft_par *par)
{
	struct backlight_device *bd;
	struct device_node *backlight;

	backlight = of_find_node_by_name(NULL, "backlight");
	if (backlight) {
		bd = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (IS_ERR(bd)) {
			dev_err(par->info->device,
				"cannot register backlight device (%ld)\n",
				PTR_ERR(bd));
			return;
		}
		par->info->bl_dev = bd;
	}
}

static struct fbtft_display display = {
	.regwidth = 8,
	.width = 240,
	.height = 320,
	.gamma_num = 2,
	.gamma_len = 14,
	.gamma = HSD20_IPS_GAMMA,
	.fbtftops = {
		.set_addr_win = set_addr_win,
		.init_display = init_display,
		.write_vmem = write_vmem,
		.set_var = set_var,
		.set_gamma = set_gamma,
		.blank = blank,
		.register_backlight = register_chip_backlight,
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "sitronix,st7789p3", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:st7789p3");
MODULE_ALIAS("platform:st7789p3");

MODULE_DESCRIPTION("FB driver for the ST7789P3 LCD Controller");
MODULE_LICENSE("GPL");