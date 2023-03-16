#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/inet.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include "pc_policy.h"
#include "cJSON.h"

struct list_head pc_rule_head = LIST_HEAD_INIT(pc_rule_head);
struct list_head pc_group_head = LIST_HEAD_INIT(pc_group_head);

DEFINE_RWLOCK(pc_policy_lock);
static void rule_init_list(pc_rule_t *rule)
{
    rule->blist.next = &rule->blist;
    rule->blist.prev = &rule->blist;
    rule->applist.next = &rule->applist;
    rule->applist.prev = &rule->applist;
}

static void rule_clean_list(pc_rule_t *rule)
{
    pc_app_t *app;
    pc_app_index_t *index;
    while (!list_empty(&rule->blist)) {
        app = list_first_entry(&rule->blist, pc_app_t, head);
        list_del(&(app->head));
        kfree(app);
    }
    while (!list_empty(&rule->applist)) {
        index = list_first_entry(&rule->applist, pc_app_index_t, head);
        list_del(&(index->head));
        kfree(index);
    }
}

static int rule_add_blist_item(pc_rule_t *rule, const char *str)
{
    pc_app_t *node = NULL;
    node = kzalloc(sizeof(pc_app_t), GFP_KERNEL);
    if (node == NULL) {
        printk("malloc feature memory error\n");
        return -1;
    } else {
        if (!pc_set_app_by_str(node, BLIST_ID, "blacklist", str)) {
            list_add(&(node->head), &rule->blist);
        } else {
            kfree(node);
            return -1;
        }
    }
    return 0;
}

static void rule_add_blist(pc_rule_t *rule, cJSON *list)
{
    int size, j;
    cJSON *item = NULL;
    if (list) {
        size = cJSON_GetArraySize(list);
        for (j = 0; j < size; j++) {
            item = cJSON_GetArrayItem(list, j);
            if (item) {
                rule_add_blist_item(rule, item->valuestring);
            }
        }
    }
}

static int rule_add_applist_item(pc_rule_t *rule, u_int32_t id)
{
    pc_app_index_t *node = NULL;
    node = kzalloc(sizeof(pc_app_index_t), GFP_KERNEL);
    if (node == NULL) {
        printk("malloc feature memory error\n");
        return -1;
    } else {
        node->app_id = id;
        list_add(&(node->head), &rule->applist);
    }
    return 0;
}

static void rule_add_applist(pc_rule_t *rule, cJSON *list)
{
    int size, j;
    cJSON *item = NULL;
    if (list) {
        size = cJSON_GetArraySize(list);
        for (j = 0; j < size; j++) {
            item = cJSON_GetArrayItem(list, j);
            if (item) {
                rule_add_applist_item(rule, item->valueint);
            }
        }
    }
}


int add_pc_rule(const char *id,  cJSON *applist, enum pc_action action,
                cJSON *blist)
{
    pc_rule_t *rule = NULL;
    rule = kzalloc(sizeof(pc_rule_t), GFP_KERNEL);
    if (rule == NULL) {
        printk("malloc pc_rule_t memory error\n");
        return -1;
    } else {
        memcpy(rule->id, id, RULE_ID_SIZE);
        rule->action = action;
        rule->refer_count = 0;
        rule_init_list(rule);
        rule_add_blist(rule, blist);
        rule_add_applist(rule, applist);
        pc_policy_write_lock();
        list_add(&rule->head, &pc_rule_head);
        pc_policy_write_unlock();
    }
    return 0;
}

int remove_pc_rule(const char *id)
{
    pc_rule_t *rule = NULL, *n;
    if (!list_empty(&pc_rule_head)) {
        list_for_each_entry_safe(rule, n, &pc_rule_head, head) {
            if (strcmp(rule->id, id) == 0) {
                if (rule->refer_count > 0) {
                    printk("refer_count of rule != 0\n");
                    return -1;
                }
                pc_policy_write_lock();
                list_del(&rule->head);
                rule_clean_list(rule);
                kfree(rule);
                pc_policy_write_unlock();
            }
        }
    }
    return 0;
}

int clean_pc_rule(void)
{
    pc_rule_t *rule = NULL;
    pc_policy_write_lock();
    while (!list_empty(&pc_rule_head)) {
        rule = list_first_entry(&pc_rule_head, pc_rule_t, head);
        list_del(&rule->head);
        rule_clean_list(rule);
        kfree(rule);
    }
    pc_policy_write_unlock();
    return 0;
}

int set_pc_rule(const char *id, cJSON *applist, enum pc_action action,
                cJSON *blist)
{
    pc_rule_t *rule = NULL, *n;
    if (!list_empty(&pc_rule_head)) {
        list_for_each_entry_safe(rule, n, &pc_rule_head, head) {
            if (strcmp(rule->id, id) == 0) {
                pc_policy_write_lock();
                memcpy(rule->id, id, RULE_ID_SIZE);
                rule_clean_list(rule);
                rule_init_list(rule);
                rule_add_blist(rule, blist);
                rule_add_applist(rule, applist);
                rule->action = action;
                pc_policy_write_unlock();
            }
        }
    }
    return 0;
}

pc_rule_t *find_rule_by_id(const char *id)
{
    pc_rule_t *rule = NULL, *n;
    pc_rule_t *ret = NULL;
    pc_policy_read_lock();
    if (!list_empty(&pc_rule_head)) {
        list_for_each_entry_safe(rule, n, &pc_rule_head, head) {
            if (strcmp(rule->id, id) == 0) {
                ret = rule;
                goto out;
            }
        }
    }
out:
    pc_policy_read_unlock();
    return ret;
}

static void group_init_list(pc_group_t *group)
{
    group->macs.next = &group->macs;
    group->macs.prev = &group->macs;
}

static void group_clean_list(pc_group_t *group)
{
    pc_mac_t *mac;
    while (!list_empty(&group->macs)) {
        mac = list_first_entry(&group->macs, pc_mac_t, head);
        list_del(&(mac->head));
        kfree(mac);
    }
}

static int mac_to_hex(const char *mac, u8 *mac_hex)
{
    u32 mac_tmp[ETH_ALEN];
    int ret = 0, i = 0;
    ret = sscanf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
                 (unsigned int *)&mac_tmp[0],
                 (unsigned int *)&mac_tmp[1],
                 (unsigned int *)&mac_tmp[2],
                 (unsigned int *)&mac_tmp[3],
                 (unsigned int *)&mac_tmp[4],
                 (unsigned int *)&mac_tmp[5]);
    if (ETH_ALEN != ret)
        return -1;
    for (i = 0; i < ETH_ALEN; i++) {
        mac_hex[i] = mac_tmp[i];
    }
    return 0;
}

static int group_add_mac_item(pc_group_t *group, const char *str)
{
    pc_mac_t *node = NULL;
    node = kzalloc(sizeof(pc_mac_t), GFP_KERNEL);
    if (node == NULL) {
        printk("malloc mac node memory error\n");
        return -1;
    } else {
        if (!mac_to_hex(str, node->mac)) {
            list_add(&(node->head), &group->macs);
        } else {
            kfree(node);
            return -1;
        }
    }
    return 0;
}

static void group_add_macs(pc_group_t *group, cJSON *list)
{
    int size, j;
    cJSON *item = NULL;
    if (list) {
        size = cJSON_GetArraySize(list);
        for (j = 0; j < size; j++) {
            item = cJSON_GetArrayItem(list, j);
            if (item) {
                group_add_mac_item(group, item->valuestring);
            }
        }
    }
}

int add_pc_group(const char *id,  cJSON *macs, const char *rule_id)
{
    pc_group_t *group = NULL;
    pc_rule_t *rule = NULL;
    group = kzalloc(sizeof(pc_group_t), GFP_KERNEL);
    if (group == NULL) {
        printk("malloc pc_group_t memory error\n");
        return -1;
    } else {
        memcpy(group->id, id, GROUP_ID_SIZE);
        group_init_list(group);
        group_add_macs(group, macs);
        rule = find_rule_by_id(rule_id);
        group->rule = rule;
        pc_policy_write_lock();
        if (rule) {
            rule->refer_count += 1;//增加规则引用计数
        }
        list_add(&group->head, &pc_group_head);
        pc_policy_write_unlock();
    }
    return 0;
}

int remove_pc_group(const char *id)
{
    pc_group_t *group = NULL, *n;
    pc_rule_t *rule = NULL;
    if (!list_empty(&pc_group_head)) {
        list_for_each_entry_safe(group, n, &pc_group_head, head) {
            if (strcmp(group->id, id) == 0) {
                rule = group->rule;
                pc_policy_write_lock();
                if (rule)
                    rule->refer_count -= 1;
                group_clean_list(group);
                list_del(&group->head);
                kfree(group);
                pc_policy_write_unlock();
            }
        }
    }
    return 0;
}

int clean_pc_group(void)
{
    pc_group_t *group = NULL;
    pc_rule_t *rule = NULL;
    pc_policy_write_lock();
    while (!list_empty(&pc_group_head)) {
        group = list_first_entry(&pc_group_head, pc_group_t, head);
        {
            rule = group->rule;
            if (rule)
                rule->refer_count -= 1;
            group_clean_list(group);
            list_del(&group->head);
            kfree(group);
        }
    }
    pc_policy_write_unlock();
    return 0;
}

int set_pc_group(const char *id,  cJSON *macs, const char *rule_id)
{
    pc_group_t *group = NULL, *n;
    pc_rule_t *rule = NULL;
    rule = find_rule_by_id(rule_id);
    PC_DEBUG("set rule %s for group %s\n", rule ? rule->id : "NULL", id);
    if (!list_empty(&pc_group_head)) {
        list_for_each_entry_safe(group, n, &pc_group_head, head) {
            if (strcmp(group->id, id) == 0) {
                PC_DEBUG("match group %s\n", group->id);
                pc_policy_write_lock();
                group_clean_list(group);
                group_add_macs(group, macs);
                if (group->rule)
                    group->rule->refer_count -= 1;//减少旧规则的引用计数
                group->rule = rule;
                if (rule)
                    rule->refer_count += 1;//增加被引用规则的引用计数
                pc_policy_write_unlock();
            }
        }
    }
    return 0;
}

static pc_group_t *_find_group_by_mac(u8 mac[ETH_ALEN])
{
    pc_group_t *group = NULL, *n;
    pc_mac_t *nmac = NULL, *nmac_n;
    if (!list_empty(&pc_group_head)) {
        list_for_each_entry_safe(group, n, &pc_group_head, head) {
            if (!list_empty(&group->macs)) {
                list_for_each_entry_safe(nmac, nmac_n, &group->macs, head) {
                    if (ether_addr_equal(nmac->mac, mac)) {
                        return group;
                    }
                }
            }
        }
    }
    return NULL;
}

pc_group_t *find_group_by_mac(u8 mac[ETH_ALEN])
{
    pc_group_t *ret = NULL;
    pc_policy_read_lock();
    ret = _find_group_by_mac(mac);
    pc_policy_read_unlock();
    return ret;
}

enum pc_action get_action_by_mac(u8 mac[ETH_ALEN])
{

    pc_group_t *group;
    pc_rule_t *rule;
    enum pc_action action;

    pc_policy_read_lock();
    group = _find_group_by_mac(mac);
    if (!group) { //TODO 设备不属于任何分组，直接通过
        action = PC_ACCEPT;
        goto EXIT;
    }
    rule = group->rule;
    if (!rule) { //TODO 设备组没有设置任何规则，直接通过
        action = PC_ACCEPT;
        goto EXIT;
    }
    action = rule->action;
EXIT:
    pc_policy_read_unlock();
    return action;
}

pc_rule_t   *get_rule_by_mac(u8 mac[ETH_ALEN], enum pc_action *action)
{
    pc_group_t *group;

    pc_policy_read_lock();
    group = _find_group_by_mac(mac);
    pc_policy_read_unlock();
    if (group) {
        if (group->rule)
            *action = group->rule->action;
        else
            *action = PC_ACCEPT;
        return group->rule;
    } else {
        if (pc_drop_anonymous) {//如果设备不属于任何分组则划分为匿名设备
            PC_LMT_DEBUG("Dtetected anonymous MAC %pM\n", mac);
            *action = PC_DROP_ANONYMOUS;
        } else
            *action = PC_ACCEPT;
        return NULL;
    }
}

static int rule_blist_print(struct seq_file *s, pc_rule_t *rule)
{
    range_value_t port_range;
    pc_app_t *app = NULL, *n;
    int i;
    seq_printf(s, "Black List:\n");
    seq_printf(s, "ID\tName\tProto\tSport\tDport\tHost_url\tRequest_url\tDataDictionary\n");
    if (!list_empty(&rule->blist)) {
        list_for_each_entry_safe(app, n, &rule->blist, head) {
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
    return 0;
}

static int rule_proc_show(struct seq_file *s, void *v)
{
    pc_rule_t *rule = NULL, *n;
    pc_app_index_t *index = NULL, *index_n;
    seq_printf(s, "ID\tAction\tRefer_count\tAPPs\n");
    pc_policy_read_lock();
    if (!list_empty(&pc_rule_head)) {
        list_for_each_entry_safe(rule, n, &pc_rule_head, head) {
            seq_printf(s, "%s\t%d\t%d\t[ ", rule->id, rule->action, rule->refer_count);
            if (!list_empty(&rule->applist)) {
                list_for_each_entry_safe(index, index_n, &rule->applist, head) {
                    seq_printf(s, "%d ", index->app_id);
                }
            }
            seq_printf(s, "]\n");
            rule_blist_print(s, rule);
            seq_printf(s, "=======================================================\n\n");
        }
    }
    pc_policy_read_unlock();
    return 0;
}

static int rule_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, rule_proc_show, NULL);
}

static int group_proc_show(struct seq_file *s, void *v)
{
    pc_group_t *group = NULL, *n;
    pc_mac_t *mac = NULL, *mac_n;
    seq_printf(s, "ID\tRule_ID\tMACs\n");
    pc_policy_read_lock();
    if (!list_empty(&pc_group_head)) {
        list_for_each_entry_safe(group, n, &pc_group_head, head) {
            seq_printf(s, "%s\t%s\t[ ", group->id, group->rule ? group->rule->id : "NULL");
            if (!list_empty(&group->macs)) {
                list_for_each_entry_safe(mac, mac_n, &group->macs, head) {
                    seq_printf(s, "%pM ", mac->mac);
                }
            }
            seq_printf(s, "]\n");
        }
    }
    pc_policy_read_unlock();
    return 0;
}

static int group_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, group_proc_show, NULL);
}

static int drop_anonymous_show(struct seq_file *s, void *v)
{
    seq_printf(s, pc_drop_anonymous ? "YES\n" : "NO\n");
    return 0;
}

static int drop_anonymous_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, drop_anonymous_show, NULL);
}

static int app_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, app_proc_show, NULL);
}

static int src_dev_show(struct seq_file *s, void *v)
{
    seq_printf(s, "%s\n", pc_src_dev);
    return 0;
}

static int src_dev_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, src_dev_show, NULL);
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 5, 0)
static const struct file_operations pc_app_fops = {
    .owner = THIS_MODULE,
    .open = app_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release_private,
};
static const struct file_operations pc_rule_fops = {
    .owner = THIS_MODULE,
    .open = rule_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release_private,
};
static const struct file_operations pc_group_fops = {
    .owner = THIS_MODULE,
    .open = group_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release_private,
};
static const struct file_operations pc_drop_anonymous_fops = {
    .owner = THIS_MODULE,
    .open = drop_anonymous_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release_private,
};
static const struct file_operations pc_src_dev_fops = {
    .owner = THIS_MODULE,
    .open = src_dev_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release_private,
};
#else
static const struct proc_ops pc_app_fops = {
    .proc_flags = PROC_ENTRY_PERMANENT,
    .proc_read = seq_read,
    .proc_open = app_proc_open,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release_private,
};
static const struct proc_ops pc_rule_fops = {
    .proc_flags = PROC_ENTRY_PERMANENT,
    .proc_read = seq_read,
    .proc_open = rule_proc_open,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release_private,
};
static const struct proc_ops pc_group_fops = {
    .proc_flags = PROC_ENTRY_PERMANENT,
    .proc_read = seq_read,
    .proc_open = group_proc_open,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release_private,
};
static const struct proc_ops pc_drop_anonymous_fops = {
    .proc_flags = PROC_ENTRY_PERMANENT,
    .proc_read = seq_read,
    .proc_open = drop_anonymous_proc_open,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release_private,
};
static const struct proc_ops pc_src_dev_fops = {
    .proc_flags = PROC_ENTRY_PERMANENT,
    .proc_read = seq_read,
    .proc_open = src_dev_proc_open,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release_private,
};
#endif



int pc_init_procfs(void)
{
    struct proc_dir_entry *proc;
    proc = proc_mkdir("parental-control", NULL);
    if (!proc) {
        PC_ERROR("can't create dir /proc/parental-control/\n");
        return -ENODEV;;
    }
    proc_create("rule", 0644, proc, &pc_rule_fops);
    proc_create("group", 0644, proc, &pc_group_fops);
    proc_create("app", 0644, proc, &pc_app_fops);
    proc_create("drop_anonymous", 0644, proc, &pc_drop_anonymous_fops);
    proc_create("src_dev", 0644, proc, &pc_src_dev_fops);
    return 0;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("luochognjun@gl-inet.com");
MODULE_DESCRIPTION("parental control module");
MODULE_VERSION("1.0");

static int __init pc_policy_init(void)
{
    if (pc_load_app_feature_list())
        return -1;
    if (pc_register_dev())
        goto free_app;
    if (pc_filter_init())
        goto free_dev;
    pc_init_procfs();
    PC_INFO("parental_control: (C) 2022 chongjun luo <luochognjun@gl-inet.com>\n");
    return 0;

free_dev:
    pc_unregister_dev();
free_app:
    pc_clean_app_feature_list();
    return -1;
}

static void pc_policy_exit(void)
{
    remove_proc_subtree("parental-control", NULL);
    pc_filter_exit();
    pc_unregister_dev();
    clean_pc_group();
    clean_pc_rule();
    pc_clean_app_feature_list();
    return;
}

module_init(pc_policy_init);
module_exit(pc_policy_exit);