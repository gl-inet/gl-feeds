#ifndef __GL_HW_INFO_H
#define __GL_HW_INFO_H

#include <linux/proc_fs.h>
#include <linux/sizes.h>
#include <linux/if.h>
#include <linux/types.h>
#include <linux/slab.h>

#define GL_HW_INFO_DRV_NAME "gl-hw-info_v1.0"
#define PROC_DIR "gl-hw-info"

#define MAC_LEN 6
#define SN_LEN 16
#define DDNS_LEN 7
#define COUNTRY_LEN 2
#define CERT_LEN 4096

struct glinet_hw_info {
    struct proc_dir_entry *parent;
    u8 device_mac[18];
    u8 device_ddns[DDNS_LEN + 1];
    u8 device_sn[SN_LEN + 1];
    u8 device_sn_bak[SN_LEN + 1];
    u8 device_cert[CERT_LEN];
    u8 *device_key;
};

extern struct glinet_hw_info gl_hw_info;

struct proc_dir_entry *create_proc_node(const char *name, const char *value);
int proc_init_gl_hw_info(void);
int proc_remove_gl_hw_info(void);

void make_factory_data(struct device_node *np);

const char *lookup_country(int iso3166);

#endif