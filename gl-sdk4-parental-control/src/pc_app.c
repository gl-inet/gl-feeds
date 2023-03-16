#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/inet.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include "pc_policy.h"
#include "pc_utils.h"
struct list_head pc_app_head = LIST_HEAD_INIT(pc_app_head);

DEFINE_RWLOCK(pc_app_lock);

static void __set_app_feature(pc_app_t *node, int appid, const char *name, int proto, int src_port,
                              port_info_t dport_info, char *host_url, char *request_url, char *dict)
{
    char *p = dict;
    char *begin = dict;
    char pos[32] = {0};
    int index = 0;
    int value = 0;
    node->app_id = appid;
    strcpy(node->app_name, name);
    node->proto = proto;
    node->dport_info = dport_info;
    node->sport = src_port;
    strcpy(node->host_url, host_url);
    strcpy(node->request_url, request_url);
    // 00:0a-01:11
    p = dict;
    begin = dict;
    index = 0;
    value = 0;
    while (*p++) {
        if (*p == '|') {
            memset(pos, 0x0, sizeof(pos));
            strncpy(pos, begin, p - begin);
            begin = p + 1;
            if (k_sscanf(pos, "%d:%x", &index, &value) == 2) {
                node->pos_info[node->pos_num].pos = index;
                node->pos_info[node->pos_num].value = value;
                node->pos_num++;
            }
        }
    }

    if (begin != dict)
        strncpy(pos, begin, p - begin);
    else
        strcpy(pos, dict);

    if (k_sscanf(pos, "%d:%x", &index, &value) == 2) {
        node->pos_info[node->pos_num].pos = index;
        node->pos_info[node->pos_num].value = value;
        node->pos_num++;
    }
}

static int __add_app_feature(int appid, const char *name, int proto, int src_port,
                             port_info_t dport_info, char *host_url, char *request_url, char *dict)
{
    pc_app_t *node = NULL;
    node = kzalloc(sizeof(pc_app_t), GFP_KERNEL);
    if (node == NULL) {
        printk("malloc feature memory error\n");
        return -1;
    } else {
        __set_app_feature(node, appid, name, proto, src_port, dport_info, host_url, request_url, dict);
        pc_app_write_lock();
        list_add(&(node->head), &pc_app_head);
        pc_app_write_unlock();
    }
    return 0;
}
static int validate_range_value(char *range_str)
{
    char *p;
    if (!range_str)
        return 0;

    p = range_str;
    while (*p) {
        if (*p == ' ' || *p == '!' || *p == '-' ||
                ((*p >= '0') && (*p <= '9'))) {
            p++;
            continue;
        } else {
            printk("error, invalid char %x\n", *p);
            return 0;
        }
    }
    return 1;
}

static int parse_range_value(char *range_str, range_value_t *range)
{
    int start, end;
    char pure_range[128] = {0};
    if (!validate_range_value(range_str)) {
        printk("validate range str failed, value = %s\n", range_str);
        return -1;
    }
    k_trim(range_str);
    if (range_str[0] == '!') {
        range->not = 1;
        strcpy(pure_range, range_str + 1);
    } else {
        range->not = 0;
        strcpy(pure_range, range_str);
    }
    k_trim(pure_range);
    if (strstr(pure_range, "-")) {
        if (2 != sscanf(pure_range, "%d-%d", &start, &end))
            return -1;
    } else {
        if (1 != sscanf(pure_range, "%d", &start))
            return -1;
        end = start;
    }
    range->start = start;
    range->end = end;
    return 0;
}

static int parse_port_info(char *port_str, port_info_t *info)
{
    char *p = port_str;
    char *begin = port_str;
    int param_num = 0;
    char one_port_buf[128] = {0};
    k_trim(port_str);
    if (strlen(port_str) == 0)
        return -1;

    while (*p++) {
        if (*p != '|')
            continue;
        memset(one_port_buf, 0x0, sizeof(one_port_buf));
        strncpy(one_port_buf, begin, p - begin);
        if (0 == parse_range_value(one_port_buf, &info->range_list[info->num])) {
            info->num++;
        }
        param_num++;
        begin = p + 1;
    }
    memset(one_port_buf, 0x0, sizeof(one_port_buf));
    strncpy(one_port_buf, begin, p - begin);
    if (0 == parse_range_value(one_port_buf, &info->range_list[info->num])) {
        info->num++;
    }
    return 0;
}

//[tcp;;443;baidu.com;;]
static int parse_app_str(pc_app_t *app, int appid, const char *name, const char *feature)
{
    char proto_str[16] = {0};
    char src_port_str[16] = {0};
    port_info_t dport_info;
    char dst_port_str[16] = {0};
    char host_url[MAX_HOST_URL_LEN] = {0};
    char request_url[MAX_REQUEST_URL_LEN] = {0};
    char dict[128] = {0};
    int proto = IPPROTO_TCP;
    const char *p = feature;
    const char *begin = feature;
    int param_num = 0;
    //int dst_port = 0;
    int src_port = 0;

    if (!name || !feature) {
        PC_ERROR("error, name or feature is null\n");
        return -1;
    }
    memset(&dport_info, 0x0, sizeof(dport_info));
    while (*p++) {
        if (*p != ';')
            continue;

        switch (param_num) {

            case PC_PROTO_PARAM_INDEX:
                strncpy(proto_str, begin, min(p - begin, sizeof(proto_str) - 1));
                break;
            case PC_SRC_PORT_PARAM_INDEX:
                strncpy(src_port_str, begin, min(p - begin, sizeof(src_port_str) - 1));
                break;
            case PC_DST_PORT_PARAM_INDEX:
                strncpy(dst_port_str, begin, min(p - begin, sizeof(dst_port_str) - 1));
                break;

            case PC_HOST_URL_PARAM_INDEX:
                strncpy(host_url, begin, min(p - begin, sizeof(host_url) - 1));
                break;

            case PC_REQUEST_URL_PARAM_INDEX:
                strncpy(request_url, begin, min(p - begin, sizeof(request_url) - 1));
                break;
        }
        param_num++;
        begin = p + 1;
    }
    if (PC_DICT_PARAM_INDEX != param_num && strlen(feature) > MIN_FEATURE_STR_LEN) {
        PC_ERROR("invalid feature:%s\n", feature);
        return -1;
    }
    strncpy(dict, begin, min(p - begin, sizeof(dict) - 1));

    if (0 == strcmp(proto_str, "tcp"))
        proto = IPPROTO_TCP;
    else if (0 == strcmp(proto_str, "udp"))
        proto = IPPROTO_UDP;
    else {
        PC_DEBUG("id %d proto %s is not support\n", appid, proto_str);
        return -1;
    }
    sscanf(src_port_str, "%d", &src_port);
    //	sscanf(dst_port_str, "%d", &dst_port);
    parse_port_info(dst_port_str, &dport_info);
    if (app)
        __set_app_feature(app, appid, name, proto, src_port, dport_info, host_url, request_url, dict);
    else
        __add_app_feature(appid, name, proto, src_port, dport_info, host_url, request_url, dict);
    return 0;
}

static int pc_add_app(int appid, const char *name, const char *feature)
{
    return parse_app_str(NULL, appid, name, feature);
}

int pc_set_app_by_str(pc_app_t *app, int appid, const char *name, const char *feature)
{
    return parse_app_str(app, appid, name, feature);
}

static void pc_init_feature(char *feature_str)
{
    int app_id;
    char app_name[128] = {0};
    char feature_buf[MAX_FEATURE_LINE_LEN] = {0};
    char *p = feature_str;
    char *pos = NULL;
    int len = 0;
    char *begin = NULL;
    char feature[MAX_FEATURE_STR_LEN];

    if (strstr(feature_str, "#"))
        return;

    sscanf(feature_str, "%d%127[^:]", &app_id, app_name);
    while (*p++) {
        if (*p == '[') {
            pos = p + 1;
            continue;
        }
        if (*p == ']' && pos != NULL) {
            len = p - pos;
        }
    }

    if (pos && len)
        strncpy(feature_buf, pos, len);
    memset(feature, 0x0, sizeof(feature));
    p = feature_buf;
    begin = feature_buf;

    while (*p++) {
        if (*p == ',') {
            memset(feature, 0x0, sizeof(feature));
            strncpy((char *)feature, begin, p - begin);

            pc_add_app(app_id, app_name, feature);
            begin = p + 1;
        }
    }
    if (p != begin) {
        memset(feature, 0x0, sizeof(feature));
        strncpy((char *)feature, begin, p - begin);
        pc_add_app(app_id, app_name, feature);
    }
}

static void load_feature_buf_from_file(char **config_buf)
{
    struct inode *inode = NULL;
    struct file *fp = NULL;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 7, 19)
    mm_segment_t fs;
#endif
    off_t size;
    fp = filp_open(PC_FEATURE_CONFIG_FILE, O_RDONLY, 0);

    if (IS_ERR(fp)) {
        printk("open feature file failed\n");
        return;
    }

    inode = fp->f_inode;
    size = inode->i_size;
    if (size == 0) {
        return;
    }
    *config_buf = (char *)kzalloc(sizeof(char) * size, GFP_KERNEL);
    if (NULL == *config_buf) {
        PC_ERROR("alloc buf fail\n");
        filp_close(fp, NULL);
        return;
    }

#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 7, 19)
    fs = get_fs();
    set_fs(KERNEL_DS);
#endif
    // 4.14rc3 vfs_read-->kernel_read
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    kernel_read(fp, *config_buf, size, &(fp->f_pos));
#else
    vfs_read(fp, *config_buf, size, &(fp->f_pos));
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 7, 19)
    set_fs(fs);
#endif
    filp_close(fp, NULL);
}

int pc_load_app_feature_list(void)
{
    char *feature_buf = NULL;
    char *p;
    char *begin;
    char line[MAX_FEATURE_LINE_LEN] = {0};

    load_feature_buf_from_file(&feature_buf);
    if (!feature_buf) {
        PC_ERROR("no app feature load\n");
        return 0;
    }
    p = begin = feature_buf;
    while (*p++) {
        if (*p == '\n') {
            if (p - begin < MIN_FEATURE_LINE_LEN || p - begin > MAX_FEATURE_LINE_LEN) {
                begin = p + 1;
                continue;
            }
            memset(line, 0x0, sizeof(line));
            strncpy(line, begin, p - begin);
            pc_init_feature(line);
            begin = p + 1;
        }
    }
    if (p != begin) {
        if (p - begin < MIN_FEATURE_LINE_LEN || p - begin > MAX_FEATURE_LINE_LEN)
            return 0;
        memset(line, 0x0, sizeof(line));
        strncpy(line, begin, p - begin);
        pc_init_feature(line);
        begin = p + 1;
    }
    if (feature_buf)
        kfree(feature_buf);
    return 0;
}

void pc_clean_app_feature_list(void)
{
    pc_app_t *node;
    pc_app_write_lock();
    while (!list_empty(&pc_app_head)) {
        node = list_first_entry(&pc_app_head, pc_app_t, head);
        list_del(&(node->head));
        kfree(node);
    }
    pc_app_write_unlock();
}

int app_proc_show(struct seq_file *s, void *v)
{
    pc_app_t *app = NULL, *n;
    range_value_t port_range;
    int i = 0;
    seq_printf(s, "ID\tName\tProto\tSport\tDport\tHost_url\tRequest_url\tDataDictionary\n");
    pc_app_read_lock();
    if (!list_empty(&pc_app_head)) {
        list_for_each_entry_safe(app, n, &pc_app_head, head) {
            seq_printf(s, "%d\t%s\t%d\t%d\t", app->app_id, app->app_name, app->proto, app->sport);
            for (i = 0; i < app->dport_info.num; i++) {
                port_range = app->dport_info.range_list[i];
                (i == 0) ? seq_printf(s, "%s", port_range.not ? "!" : "") :
                seq_printf(s, "%s", port_range.not ? "|!" : "|");
                (port_range.start == port_range.end) ?
                seq_printf(s, "%d", port_range.start) :
                seq_printf(s, "%d-%d", port_range.start, port_range.end);
            }
            if (app->dport_info.num)
                seq_printf(s, "\t");
            seq_printf(s, "%s\t%s", app->host_url, app->request_url);

            for (i = 0; i < app->pos_num; i++) {
                seq_printf(s, "%s[%d]=0x%x", (i == 0) ? "\t" : "&&", app->pos_info[i].pos, app->pos_info[i].value);
            }
            seq_printf(s, "\n");
        }
    }
    pc_app_read_unlock();
    return 0;
}