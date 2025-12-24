#include <lauxlib.h>
#include <lua.h>
#include <iwinfo.h>

static int lua_iwinfo_info(lua_State *L)
{
    const char *ifname = luaL_checkstring(L, 1);
    const struct iwinfo_ops *iw = iwinfo_backend(ifname);
    char buf[IWINFO_BUFSIZE];
    int num;

    if (!iw) {
        lua_pushnil(L);
        lua_pushfstring(L, "No such wireless device: %s", ifname);
        return 2;
    }

    lua_newtable(L);

    memset(buf, 0, sizeof(buf));

    if (!iw->ssid(ifname, buf) && buf[0]) {
        lua_pushstring(L, buf);
        lua_setfield(L, -2, "ssid");
    }

    memset(buf, 0, sizeof(buf));

    if (!iw->bssid(ifname, buf) && buf[0]) {
        lua_pushstring(L, buf);
        lua_setfield(L, -2, "bssid");
    }

    memset(buf, 0, sizeof(buf));

    if (!iw->channel(ifname, &num)) {
        lua_pushinteger(L, num);
        lua_setfield(L, -2, "channel");
    }

    if (!iw->signal(ifname, &num)) {
        lua_pushinteger(L, num);
        lua_setfield(L, -2, "signal");
    }

    if (iw->htmode && !iw->htmode(ifname, &num)) {
        const char *htmode = iwinfo_htmode_name(num);
        lua_pushstring(L, htmode ? htmode : "unknown");
        lua_setfield(L, -2, "htmode");
    }

    if (!iw->phyname(ifname, buf)) {
        lua_pushstring(L, buf);
        lua_setfield(L, -2, "phyname");
    }

    iwinfo_finish();

    return 1;
}

static int lua_iwinfo_freqlist(lua_State *L)
{
    const char *ifname = luaL_checkstring(L, 1);
    const struct iwinfo_ops *iw = iwinfo_backend(ifname);
    struct iwinfo_freqlist_entry *e;
    char buf[IWINFO_BUFSIZE];
    int i, j, len, freq;
    int ret = 1;

    if (!iw) {
        lua_pushnil(L);
        lua_pushfstring(L, "No such wireless device: %s", ifname);
        return 2;
    }

    if (iw->freqlist(ifname, buf, &len) || len <= 0) {
        ret = 2;
        lua_pushnil(L);
        lua_pushfstring(L, "No frequency information available");
        goto done;
    }

    lua_newtable(L);

    for (i = 0, j = 1; i < len; i += sizeof(struct iwinfo_freqlist_entry)) {
        e = (struct iwinfo_freqlist_entry *)&buf[i];

        lua_newtable(L);

        /* MHz */
        lua_pushinteger(L, e->mhz);
        lua_setfield(L, -2, "mhz");

        /* Channel */
        lua_pushinteger(L, e->channel);
        lua_setfield(L, -2, "channel");

        /* Restricted (DFS/TPC/Radar) */
        lua_pushboolean(L, e->restricted);
        lua_setfield(L, -2, "restricted");

        lua_rawseti(L, -2, j++);
    }

done:
    iwinfo_finish();
    return ret;
}

static int lua_iwinfo_assoclist(lua_State *L)
{
    const char *ifname = luaL_checkstring(L, 1);
    const struct iwinfo_ops *iw = iwinfo_backend(ifname);
    struct iwinfo_assoclist_entry *e;
    char buf[IWINFO_BUFSIZE];
    char macstr[18];
    int i, j, len, freq;
    int ret = 1;

    if (!iw) {
        lua_pushnil(L);
        lua_pushfstring(L, "No such wireless device: %s", ifname);
        return 2;
    }

    if (iw->assoclist(ifname, buf, &len) || len <= 0) {
        ret = 2;
        lua_pushnil(L);
        lua_pushfstring(L, "No assoclist information available");
        goto done;
    }

    lua_newtable(L);

    for (i = 0; i < len; i += sizeof(struct iwinfo_assoclist_entry)) {
        e = (struct iwinfo_assoclist_entry *)&buf[i];

        sprintf(macstr, "%02x:%02x:%02x:%02x:%02x:%02x",
                e->mac[0], e->mac[1], e->mac[2],
                e->mac[3], e->mac[4], e->mac[5]);

        lua_newtable(L);

        lua_pushinteger(L, e->signal);
        lua_setfield(L, -2, "signal");

        lua_pushinteger(L, e->noise);
        lua_setfield(L, -2, "noise");

        lua_pushinteger(L, e->inactive);
        lua_setfield(L, -2, "inactive");

        lua_pushinteger(L, e->rx_packets);
        lua_setfield(L, -2, "rx_packets");

        lua_pushinteger(L, e->tx_packets);
        lua_setfield(L, -2, "tx_packets");

        if (e->thr) {
            lua_pushinteger(L, e->thr);
            lua_setfield(L, -2, "expected_throughput");
        }

        lua_setfield(L, -2, macstr);
    }

done:
    iwinfo_finish();
    return ret;
}

static const luaL_Reg funcs[] = {
    {"info", lua_iwinfo_info},
    {"freqlist", lua_iwinfo_freqlist},
    {"assoclist", lua_iwinfo_assoclist},
    {NULL, NULL}
};

int luaopen_iwinfo(lua_State *L)
{
    luaL_newlib(L, funcs);

    return 1;
}
