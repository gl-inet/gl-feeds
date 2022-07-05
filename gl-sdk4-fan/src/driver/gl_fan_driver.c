#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>

#define GL_FAN_DRV_NAME "gl-fan_v1.0"

typedef struct {
    struct workqueue_struct *g_work;
    struct delayed_work g_delayed_work;
    struct class *g_class;
    unsigned int count;
    unsigned int gpio;
    bool refresh;
} gl_fan_t;

static gl_fan_t gl_fan;

static void g_delayed_work_function(struct work_struct *work)
{
    gl_fan.refresh = 0;
    disable_irq(gpio_to_irq(gl_fan.gpio));
}

static irq_handler_t handle_gpio_irq(unsigned int irq, void *device, struct pt_regs *registers)
{
    gl_fan.count++;
    return (irq_handler_t)IRQ_HANDLED;
}

static ssize_t fan_speed_show(struct class *class, struct class_attribute *attr, char *buf)
{
    if (gl_fan.refresh) {
        return sprintf(buf, "refreshing...\n");
    } else {
        return sprintf(buf, "%d\n", (int)(30 * gl_fan.count));
    }
}

static ssize_t fan_speed_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    if (!strstr(buf, "refresh")) {
        printk(KERN_ERR "please input 'refresh' %s\n", buf);
        return -EBADRQC;
    } else {
        gl_fan.refresh = true;
        gl_fan.count = 0 ;
        enable_irq(gpio_to_irq(gl_fan.gpio));
        queue_delayed_work(gl_fan.g_work, &gl_fan.g_delayed_work, msecs_to_jiffies(1000));
        return count;
    }
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
static CLASS_ATTR(fan_speed, 0664, fan_speed_show, fan_speed_store);
#else
static const struct class_attribute class_attr_fan_speed =
    __ATTR(fan_speed, 0664, fan_speed_show, fan_speed_store);
#endif

static int gl_fan_probe(struct platform_device *pdev)
{
#ifdef CONFIG_OF
    struct device_node *np = pdev->dev.of_node;

    if (np) {
        if (of_property_read_u32_array(np, "fan_speed_gpio", &gl_fan.gpio, 1)) {
            printk("can't find fan_speed_gpio\n");
            return -1;
        } else {
            printk("fan_speed_gpio is %d\n", gl_fan.gpio);
        }
    } else {
        printk("can't find gl_fan node\n");
        return  -2;
    }

    gl_fan.count = 0;

    gl_fan.g_work = create_workqueue("workqueue");
    if (!gl_fan.g_work) {
        printk(KERN_ERR "Create workqueue failed!\n");
        return -3;
    }
    INIT_DELAYED_WORK(&gl_fan.g_delayed_work, g_delayed_work_function);

    if (gpio_request(gl_fan.gpio, "fan speed") != 0) {
        flush_workqueue(gl_fan.g_work);
        destroy_workqueue(gl_fan.g_work);
        printk(KERN_ERR "request gpio%d failed!\n", gl_fan.gpio);
        return -4;
    }

    if (gpio_direction_input(gl_fan.gpio) != 0) {
        flush_workqueue(gl_fan.g_work);
        destroy_workqueue(gl_fan.g_work);
        gpio_free(gl_fan.gpio);
        printk(KERN_ERR "set gpio%d INPUT failed!\n", gl_fan.gpio);
        return -5;
    }

    if (request_irq(gpio_to_irq(gl_fan.gpio), (irq_handler_t) handle_gpio_irq, IRQF_TRIGGER_RISING, "fan speed", NULL) != 0) {
        flush_workqueue(gl_fan.g_work);
        destroy_workqueue(gl_fan.g_work);
        gpio_free(gl_fan.gpio);
        printk(KERN_ERR "request gpio%d irq failed!\n", gl_fan.gpio);
        return -6;
    }
    disable_irq(gpio_to_irq(gl_fan.gpio));

    gl_fan.g_class = class_create(THIS_MODULE, "fan");
    if (class_create_file(gl_fan.g_class, &class_attr_fan_speed) != 0) {
        free_irq(gpio_to_irq(gl_fan.gpio), NULL);
        gpio_free(gl_fan.gpio);
        cancel_delayed_work_sync(&gl_fan.g_delayed_work);
        if (gl_fan.g_work != NULL) {
            flush_workqueue(gl_fan.g_work);
            destroy_workqueue(gl_fan.g_work);
        }
        printk(KERN_ERR "fail to creat class file\n");
        return -7;
    }
#endif
    printk("install gl_fan\n");

    return 0;
}

static int gl_fan_remove(struct platform_device *pdev)
{
    free_irq(gpio_to_irq(gl_fan.gpio), NULL);
    gpio_free(gl_fan.gpio);
    cancel_delayed_work_sync(&gl_fan.g_delayed_work);
    if (gl_fan.g_work != NULL) {
        flush_workqueue(gl_fan.g_work);
        destroy_workqueue(gl_fan.g_work);
    }
    if (gl_fan.g_class != NULL) {
        class_destroy(gl_fan.g_class);
    }
    printk("remove gl_fan\n");
    return 0;
}

static const struct of_device_id gl_fan_match[] = {
    { .compatible = "gl-fan" },
    {}
};

static struct platform_driver gl_fan_driver = {
    .probe		= gl_fan_probe,
    .remove		= gl_fan_remove,
    .driver = {
        .name	= GL_FAN_DRV_NAME,
        .of_match_table = gl_fan_match,
    }
};

static int __init gl_fan_init(void)
{
    return platform_driver_register(&gl_fan_driver);
}

static void __exit gl_fan_exit(void)
{
    platform_driver_unregister(&gl_fan_driver);
}

module_init(gl_fan_init);
module_exit(gl_fan_exit);

MODULE_AUTHOR("xinfa deng <xinfa.deng@gl-inet.com>");
MODULE_LICENSE("GPL");
