#ifndef __GL_HW_INFO_H
#define __GL_HW_INFO_H

#include <linux/proc_fs.h>
#include <linux/sizes.h>
#include <linux/if.h>
#include <linux/types.h>
#include <linux/slab.h>

#define GL_HW_INFO_DRV_NAME "gl-hw-info_v1.0"
#define PROC_DIR "gl-hw-info"

#define GL_DEVICE_FILE_PATH        "/lib/firmware/sn.data"
#define GL_DEVICE_BUFFER_SIZE      1024

#define GL_DEVICE_SN               "device_sn="
#define GL_DEVICE_DDNS             "device_ddns="
#define GL_DEVICE_MAC              "device_mac="
#define GL_DEVICE_MODEL            "model="
#define GL_DEVICE_COUNTRY_CODE     "country_code="
#define GL_DEVICE_LAN              "lan="
#define GL_DEVICE_WAN              "wan="

#define MAC_LEN 6
#define SN_LEN 16
#define DDNS_LEN 7
#define COUNTRY_LEN 2
#define FIRMWARE_LEN 2
#define USB_POWER_ENABLE 1
#define CERT_LEN 4096
#define SUBMODEL_LEN 16

struct glinet_hw_info {
    struct proc_dir_entry *parent;
    u8 device_mac[18];
    u8 device_ddns[DDNS_LEN + 1];
    u8 firmware_type[FIRMWARE_LEN + 1];
    u8 device_sn[SN_LEN + 1];
    u8 device_sn_bak[SN_LEN + 1];
    u8 device_submodel[SUBMODEL_LEN + 1];
    u8 device_cert[CERT_LEN + 1];
    u8 *device_key;
    u8 usb_power_mode;
};

extern struct glinet_hw_info gl_hw_info;

struct proc_dir_entry *create_proc_node(const char *name, const char *value);
int proc_init_gl_hw_info(void);
int proc_remove_gl_hw_info(void);

void make_factory_data(struct device_node *np);

const char *lookup_country(int iso3166);

#endif
