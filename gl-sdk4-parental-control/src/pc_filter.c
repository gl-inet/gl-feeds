#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <net/tcp.h>
#include <linux/netfilter.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_acct.h>
#include <linux/skbuff.h>
#include <net/ip.h>
#include <linux/types.h>
#include <net/sock.h>
#include <linux/etherdevice.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include "pc_policy.h"
#include "pc_utils.h"


int parse_flow_proto(struct sk_buff *skb, flow_info_t *flow)
{
    struct tcphdr *tcph = NULL;
    struct udphdr *udph = NULL;
    struct iphdr *iph = NULL;
    if (!skb)
        return -1;
    iph = ip_hdr(skb);
    if (!iph)
        return -1;
    flow->src = iph->saddr;
    flow->dst = iph->daddr;
    flow->l4_protocol = iph->protocol;
    switch (iph->protocol) {
        case IPPROTO_TCP:
            tcph = (struct tcphdr *)(iph + 1);
            flow->l4_data = skb->data + iph->ihl * 4 + tcph->doff * 4;
            flow->l4_len = ntohs(iph->tot_len) - iph->ihl * 4 - tcph->doff * 4;
            flow->dport = htons(tcph->dest);
            flow->sport = htons(tcph->source);
            return 0;
        case IPPROTO_UDP:
            udph = (struct udphdr *)(iph + 1);
            flow->l4_data = skb->data + iph->ihl * 4 + 8;
            flow->l4_len = ntohs(udph->len) - 8;
            flow->dport = htons(udph->dest);
            flow->sport = htons(udph->source);
            return 0;
        case IPPROTO_ICMP:
            break;
        default:
            return -1;
    }
    return -1;
}

int dpi_https_proto(flow_info_t *flow)
{
    int i;
    short url_len = 0;
    char *p;
    int data_len;

    if (NULL == flow) {
        PC_ERROR("flow is NULL\n");
        return -1;
    }
    p = flow->l4_data;
    data_len = flow->l4_len;

    if (NULL == p || data_len == 0) {
        return -1;
    }
    if (!(p[0] == 0x16 && p[1] == 0x03 && p[2] == 0x01))
        return -1;


    for (i = 0; i < data_len; i++) {
        if (i + HTTPS_URL_OFFSET >= data_len) {
            return -1;
        }

        if (p[i] == 0x0 && p[i + 1] == 0x0 && p[i + 2] == 0x0 && p[i + 3] != 0x0) {
            // 2 bytes
            memcpy(&url_len, p + i + HTTPS_LEN_OFFSET, 2);
            if (ntohs(url_len) <= 0 || ntohs(url_len) > data_len) {
                continue;
            }
            if (i + HTTPS_URL_OFFSET + ntohs(url_len) < data_len) {
                flow->https.match = PC_TRUE;
                flow->https.url_pos = p + i + HTTPS_URL_OFFSET;
                flow->https.url_len = ntohs(url_len);
                return 0;
            }
        }
    }
    return -1;
}

void dpi_http_proto(flow_info_t *flow)
{
    int i = 0;
    int start = 0;
    char *data = NULL;
    int data_len = 0;
    if (!flow) {
        PC_ERROR("flow is null\n");
        return;
    }
    if (flow->l4_protocol != IPPROTO_TCP) {
        return;
    }

    data = flow->l4_data;
    data_len = flow->l4_len;
    if (data_len < MIN_HTTP_DATA_LEN) {
        return;
    }
    if (flow->sport != 80 && flow->dport != 80)
        return;
    for (i = 0; i < data_len; i++) {
        if (data[i] == 0x0d && data[i + 1] == 0x0a) {
            if (0 == memcmp(&data[start], "POST ", 5)) {
                flow->http.match = PC_TRUE;
                flow->http.method = HTTP_METHOD_POST;
                flow->http.url_pos = data + start + 5;
                flow->http.url_len = i - start - 5;
            } else if (0 == memcmp(&data[start], "GET ", 4)) {
                flow->http.match = PC_TRUE;
                flow->http.method = HTTP_METHOD_GET;
                flow->http.url_pos = data + start + 4;
                flow->http.url_len = i - start - 4;
            } else if (0 == memcmp(&data[start], "Host:", 5)) {
                flow->http.host_pos = data + start + 6;
                flow->http.host_len = i - start - 6;
            }
            if (data[i + 2] == 0x0d && data[i + 3] == 0x0a) {
                flow->http.data_pos = data + i + 4;
                flow->http.data_len = data_len - i - 4;
                break;
            }
            // 0x0d 0x0a
            start = i + 2;
        }
    }
}

int pc_match_port(port_info_t *info, int port)
{
    int i;
    int with_not = 0;
    if (info->num == 0)
        return 1;
    for (i = 0; i < info->num; i++) {
        if (info->range_list[i].not) {
            with_not = 1;
            break;
        }
    }
    for (i = 0; i < info->num; i++) {
        if (with_not) {
            if (info->range_list[i].not && port >= info->range_list[i].start
                    && port <= info->range_list[i].end) {
                return 0;
            }
        } else {
            if (port >= info->range_list[i].start
                    && port <= info->range_list[i].end) {
                return 1;
            }
        }
    }
    if (with_not)
        return 1;
    else
        return 0;
}

int pc_match_by_pos(flow_info_t *flow, pc_app_t *node)
{
    int i;
    unsigned int pos = 0;

    if (!flow || !node)
        return PC_FALSE;
    if (node->pos_num > 0) {
        for (i = 0; i < node->pos_num; i++) {
            // -1
            if (node->pos_info[i].pos < 0) {
                pos = flow->l4_len + node->pos_info[i].pos;
            } else {
                pos = node->pos_info[i].pos;
            }
            if (pos >= flow->l4_len) {
                return PC_FALSE;
            }
            if (flow->l4_data[pos] != node->pos_info[i].value) {
                return PC_FALSE;
            }
        }
        PC_DEBUG("match by pos, appid=%d\n", node->app_id);
        return PC_TRUE;
    }
    return PC_FALSE;
}

int pc_match_by_url(flow_info_t *flow, pc_app_t *node)
{
    char reg_url_buf[MAX_URL_MATCH_LEN] = {0};

    if (!flow || !node)
        return PC_FALSE;
    // match host or https url
    if (flow->https.match == PC_TRUE && flow->https.url_pos) {
        if (flow->https.url_len >= MAX_URL_MATCH_LEN)
            strncpy(reg_url_buf, flow->https.url_pos, MAX_URL_MATCH_LEN - 1);
        else
            strncpy(reg_url_buf, flow->https.url_pos, flow->https.url_len);
    } else if (flow->http.match == PC_TRUE && flow->http.host_pos) {
        if (flow->http.host_len >= MAX_URL_MATCH_LEN)
            strncpy(reg_url_buf, flow->http.host_pos, MAX_URL_MATCH_LEN - 1);
        else
            strncpy(reg_url_buf, flow->http.host_pos, flow->http.host_len);
    }
    if (strlen(reg_url_buf) > 0 && strlen(node->host_url) > 0 && regexp_match(node->host_url, reg_url_buf)) {
        PC_DEBUG("match url:%s	 reg = %s, appid=%d\n",
                 reg_url_buf, node->host_url, node->app_id);
        return PC_TRUE;
    }

    // match request url
    if (flow->http.match == PC_TRUE && flow->http.url_pos) {
        memset(reg_url_buf, 0x0, sizeof(reg_url_buf));
        if (flow->http.url_len >= MAX_URL_MATCH_LEN)
            strncpy(reg_url_buf, flow->http.url_pos, MAX_URL_MATCH_LEN - 1);
        else
            strncpy(reg_url_buf, flow->http.url_pos, flow->http.url_len);
        if (strlen(reg_url_buf) > 0 && strlen(node->request_url) && regexp_match(node->request_url, reg_url_buf)) {
            PC_DEBUG("match request:%s   reg:%s appid=%d\n",
                     reg_url_buf, node->request_url, node->app_id);
            return PC_TRUE;
        }
    }
    return PC_FALSE;
}

int pc_match_one(flow_info_t *flow, pc_app_t *node)
{
    int ret = PC_FALSE;
    if (!flow || !node) {
        PC_ERROR("node or flow is NULL\n");
        return PC_FALSE;
    }
    if (node->proto > 0 && flow->l4_protocol != node->proto)
        return PC_FALSE;
    if (flow->l4_len == 0)
        return PC_FALSE;

    if (node->sport != 0 && flow->sport != node->sport) {
        return PC_FALSE;
    }

    if (!pc_match_port(&node->dport_info, flow->dport)) {
        return PC_FALSE;
    }

    if (strlen(node->request_url) > 0 ||
            strlen(node->host_url) > 0) {
        ret = pc_match_by_url(flow, node);
    } else if (node->pos_num > 0) {
        ret = pc_match_by_pos(flow, node);
    } else {
        PC_DEBUG("node is empty, match sport:%d,dport:%d, appid = %d\n",
                 node->sport, node->dport, node->app_id);
        return PC_TRUE;
    }
    return ret;
}

static int app_in_rule(u_int32_t app, pc_rule_t *rule)
{
    pc_app_index_t *node, *n;
    if (app < MAX_APP_IN_CLASS) {
        return PC_FALSE;
    }
    if (!list_empty(&rule->applist)) {
        list_for_each_entry_safe(node, n, &rule->applist, head) {
            if (app == node->app_id)
                return PC_TRUE;
        }
        app = app / MAX_APP_IN_CLASS; //如果单个应用不匹配，进一步检查是否匹配应用类型
        list_for_each_entry_safe(node, n, &rule->applist, head) {
            if (app == node->app_id)
                return PC_TRUE;
        }
    }
    return PC_FALSE;
}

static int match_blist_app(flow_info_t *flow, pc_rule_t *rule)
{
    pc_app_t *app, *n;
    if (!list_empty(&rule->blist)) {
        list_for_each_entry_safe(app, n, &rule->blist, head) {
            if (pc_match_one(flow, app)) {
                PC_LMT_DEBUG("rule %s match blist app %s from mac %pM\n", rule->id, app->app_name, flow->smac);
                return PC_TRUE;
            }
        }
    }
    return PC_FALSE;
}

int app_filter_match(flow_info_t *flow, pc_rule_t *rule)
{
    pc_app_t *n, *node;
    pc_policy_read_lock();
    pc_app_read_lock();
    if (rule == NULL || flow == NULL)
        goto EXIT;
    if (match_blist_app(flow, rule)) {
        flow->drop = PC_TRUE;
        PC_LMT_DEBUG("match blist from mac %pM, policy is %s\n", flow->smac, flow->drop ? "DROP" : "ACCEPT");
        goto EXIT;
    }
    if (!list_empty(&pc_app_head)) {
        list_for_each_entry_safe(node, n, &pc_app_head, head) {

            if (!app_in_rule(node->app_id, rule)) {
                continue;
            }
            if (pc_match_one(flow, node)) {
                if (rule->action == PC_POLICY_DROP) {
                    flow->drop = PC_TRUE;
                } else {
                    flow->drop = PC_FALSE;
                }
                strcpy(flow->app_name, node->app_name);
                flow->app_id = node->app_id;
                PC_LMT_DEBUG("match app %d from mac %pM, policy is %s\n", node->app_id, flow->smac, flow->drop ? "DROP" : "ACCEPT");
                goto EXIT;
            }
        }
    }
    flow->drop = PC_FALSE;
EXIT:
    pc_app_read_unlock();
    pc_policy_read_unlock();
    return 0;
}

int dpi_main(struct sk_buff *skb, flow_info_t *flow)
{
    dpi_http_proto(flow);
    dpi_https_proto(flow);
    /*if (TEST_MODE())
    	dump_flow_info(flow);*/
    return 0;
}

void pc_get_smac(struct sk_buff *skb,  u8 smac[ETH_ALEN])
{
    struct ethhdr *ethhdr = NULL;
    ethhdr = eth_hdr(skb);
    if (ethhdr)
        memcpy(smac, ethhdr->h_source, ETH_ALEN);
    /*else
        memcpy(smac, &skb->cb[40], ETH_ALEN);*/
}

static int check_source_net_dev(struct sk_buff *skb)
{
    char nstr[MAX_SRC_DEVNAME_SIZE] = {0};
    char *ptr;
    char *item = NULL;
    struct net_device *netdev = skb->dev;

    if (!netdev)
        return PC_FALSE;
    PC_LMT_DEBUG("get package from %s\n", netdev->name);
    if (0 == strlen(pc_src_dev)) {
        PC_LMT_DEBUG("match any netdev\n");
        return PC_TRUE;
    }
    strcpy(nstr, pc_src_dev);
    ptr = nstr;
    while (ptr) {
        item = strsep(&ptr, " ");
        if (0 == strcmp(item, netdev->name)) {
            PC_LMT_DEBUG("match net dev %s\n", netdev->name);
            return PC_TRUE;
        }
    }
    return PC_FALSE;
}

u_int32_t pc_filter_hook_handle(struct sk_buff *skb, struct net_device *dev)
{
    u_int32_t ret;
    flow_info_t flow;
    pc_rule_t *rule;
    enum ip_conntrack_info ctinfo;
    struct nf_conn *ct = NULL;
    enum pc_action action;
    if (!check_source_net_dev(skb)) {
        ret = NF_ACCEPT;
        goto EXIT;
    }
    ct = nf_ct_get(skb, &ctinfo);
    /*if (ct) {
        PC_LMT_DEBUG("ctinfo %d\n", ctinfo);
    } else {
        PC_LMT_DEBUG("no ctinfo found\n");
    }*/

    memset((char *)&flow, 0x0, sizeof(flow_info_t));
    pc_get_smac(skb,  flow.smac);
    if (is_zero_ether_addr(flow.smac) || is_broadcast_ether_addr(flow.smac)) {
        ret = NF_ACCEPT;
        goto EXIT;
    }

    rule = get_rule_by_mac(flow.smac, &action);
    switch (action) {
        case PC_DROP:
            PC_LMT_DEBUG("from mac %pM action is DROP\n", flow.smac);
            ret = NF_DROP;
            goto EXIT;
        case PC_ACCEPT:
            PC_LMT_DEBUG("from mac %pM action is ACCEPT\n", flow.smac);
            ret = NF_ACCEPT;
            goto EXIT;
        case PC_DROP_ANONYMOUS:
            if (ctinfo == IP_CT_ESTABLISHED || ctinfo == IP_CT_RELATED || ctinfo == IP_CT_IS_REPLY) {
                PC_LMT_DEBUG("from mac %pM action is match ct\n", flow.smac);
                ret = NF_ACCEPT;
            } else {
                PC_LMT_DEBUG("from mac %pM action is ANONYMOUS DROP\n", flow.smac);
                ret = NF_DROP;
            }
            goto EXIT;
        case PC_POLICY_DROP:
            PC_LMT_DEBUG("from mac %pM action is POLICY DROP\n", flow.smac);
        case PC_POLICY_ACCEPT:
            break;
        default:
            ret = NF_ACCEPT;
            goto EXIT;
    }

    if (rule == NULL) {
        PC_LMT_DEBUG("from mac %pM rule is NULL,ACCEPT\n", flow.smac);
        ret = NF_ACCEPT;
        goto EXIT;
    }

    if (parse_flow_proto(skb, &flow) < 0) {
        PC_LMT_DEBUG("from mac %pM parese proto failed, ACCEPT\n", flow.smac);
        ret = NF_ACCEPT;
        goto EXIT;
    }

    if (0 != dpi_main(skb, &flow)) {
        PC_LMT_DEBUG("from mac %pM dpi failed, ACCEPT\n", flow.smac);
        ret = NF_ACCEPT;
        goto EXIT;
    }

    app_filter_match(&flow, rule);

    if (flow.app_id != 0) {
        PC_LMT_DEBUG("match %s %pI4(%d)--> %pI4(%d) len = %d, %d\n ", IPPROTO_TCP == flow.l4_protocol ? "tcp" : "udp",
                     &flow.src, flow.sport, &flow.dst, flow.dport, skb->len, flow.app_id);
    }
    if (flow.drop) {
        PC_LMT_DEBUG("Drop app %s flow, appid is %d\n", flow.app_name, flow.app_id);
        ret =  NF_DROP;
        goto EXIT;
    }
    ret = NF_ACCEPT;
EXIT:
    return ret;
}



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
static u_int32_t pc_filter_hook(void *priv,
                                struct sk_buff *skb,
                                const struct nf_hook_state *state)
{
#else
static u_int32_t pc_filter_hook(unsigned int hook,
                                struct sk_buff *skb,
                                const struct net_device *in,
                                const struct net_device *out,
                                int (*okfn)(struct sk_buff *))
{
#endif
    return pc_filter_hook_handle(skb, skb->dev);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
static struct nf_hook_ops pc_filter_ops[] __read_mostly = {
    {
        .hook = pc_filter_hook,
        .pf = PF_INET,
        .hooknum = NF_INET_FORWARD,
        .priority = NF_IP_PRI_MANGLE + 1,

    },
};
#else
static struct nf_hook_ops pc_filter_ops[] __read_mostly = {
    {
        .hook = pc_filter_hook,
        .owner = THIS_MODULE,
        .pf = PF_INET,
        .hooknum = NF_INET_FORWARD,
        .priority = NF_IP_PRI_MANGLE + 1,
    },
};
#endif

#ifdef CONFIG_SHORTCUT_FE
extern int (*gl_parental_control_handle)(struct sk_buff *skb);
extern int (*athrs_fast_nat_recv)(struct sk_buff *skb);
static int pc_handle_shortcut_fe(struct sk_buff *skb)
{
    int (*fast_recv)(struct sk_buff * skb);
    const struct iphdr *iph = ip_hdr(skb);
    struct rtable *rt;
    int err;

    rcu_read_lock();
    fast_recv = rcu_dereference(athrs_fast_nat_recv);
    rcu_read_unlock();
    if (!fast_recv)//no shortcut module installed
        return NET_RX_SUCCESS;

    err = ip_route_input_noref(skb, iph->daddr, iph->saddr,
                               iph->tos, skb->dev);
    if (unlikely(err))
        return NET_RX_SUCCESS;
    rt = skb_rtable(skb);
    if (rt == NULL)
        return NET_RX_SUCCESS;
    if (rt->rt_type == RTN_MULTICAST || rt->rt_type ==  RTN_BROADCAST
            || rt->rt_type ==  RTN_LOCAL) {
        return NET_RX_SUCCESS;
    }

    switch (pc_filter_hook_handle(skb, skb->dev)) {
        case NF_ACCEPT:
            return NET_RX_SUCCESS;
        case NF_DROP:
            return NET_RX_DROP;
        default:
            return NET_RX_SUCCESS;
    }
}
static void pc_rpc_pointer_init(void)
{
    int (*test)(struct sk_buff * skb);
    rcu_read_lock();
    test = rcu_dereference(gl_parental_control_handle);
    rcu_read_unlock();
    if (!test) {
        RCU_INIT_POINTER(gl_parental_control_handle, pc_handle_shortcut_fe);
    }
}

static void pc_rpc_pointer_exit(void)
{
    RCU_INIT_POINTER(gl_parental_control_handle, NULL);
    //wait for all rcu call complete
    rcu_barrier();
}
#else
static void pc_rpc_pointer_init(void) {}
static void pc_rpc_pointer_exit(void) {}
#endif

int pc_filter_init(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
    nf_register_net_hooks(&init_net, pc_filter_ops, ARRAY_SIZE(pc_filter_ops));
#else
    nf_register_hooks(pc_filter_ops, ARRAY_SIZE(pc_filter_ops));
#endif
    pc_rpc_pointer_init();
    return 0;
}

void pc_filter_exit(void)
{
    pc_rpc_pointer_exit();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
    nf_unregister_net_hooks(&init_net, pc_filter_ops, ARRAY_SIZE(pc_filter_ops));
#else
    nf_unregister_hooks(pc_filter_ops, ARRAY_SIZE(pc_filter_ops));
#endif
    return;
}

