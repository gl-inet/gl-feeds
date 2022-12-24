#include <sys/socket.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <net/if.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <lauxlib.h>
#include <errno.h>

#include "lua_compat.h"
#include "wext.h"
#include "mtk.h"

int iwpriv_set(const char *ifname, const char *option, const char *value)
{
    struct iwreq wrq = {};
    char buf[256];
    size_t len;
    int s;

    snprintf(buf, sizeof(buf), "%s=%s", option, value);

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return -1;

    strcpy(wrq.ifr_name, ifname);
    len = strlen(buf);
    wrq.u.data.pointer = buf;
    wrq.u.data.length = len;

    ioctl(s, RTPRIV_IOCTL_SET, &wrq);

    close(s);

    return  0;
}

int iwpriv_show(const char *ifname, char *buf, int len)
{
    struct iwreq wrq = {};
    int s;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return -1;

    strcpy(wrq.ifr_name, ifname);

    wrq.u.data.pointer = buf;
    wrq.u.data.length = len;

    ioctl(s, RTPRIV_IOCTL_SHOW, &wrq);

    close(s);

    return 0;
}

static int lua_iwpriv_set(lua_State *L)
{
    const char *ifname = luaL_checkstring(L, 1);
    const char *option = luaL_checkstring(L, 2);
    const char *value = lua_tostring(L, 3);
    int ret;

    ret = iwpriv_set(ifname, option, value ? value : "");

    lua_pushinteger(L, ret);

    return 1;
}

static int lua_iwpriv_show(lua_State *L)
{
    const char *ifname = luaL_checkstring(L, 1);
    const char *option = luaL_checkstring(L, 2);
    char buf[1024] = "";

    strncpy(buf, option, sizeof(buf) - 1);

    iwpriv_show(ifname, buf, sizeof(buf));

    lua_pushstring(L, buf);

    return 1;
}

static int lua_iwpriv_get_site_survey2(lua_State *L)
{
    const char *ifname = luaL_checkstring(L, 1);
    int index = lua_tointeger(L, 2);
    char data[20 * 1024 + 1] = "";
    struct iwreq wrq = {};
    int sock;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        return 2;
    }

    snprintf(wrq.ifr_name, sizeof(wrq.ifr_name), "%s", ifname);
    snprintf(data, sizeof(data), "%d", index);

    wrq.u.data.pointer = data;
    wrq.u.data.length = sizeof(data);

    if (ioctl(sock, RTPRIV_IOCTL_GSITESURVEY, &wrq) < 0) {
        close(sock);
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        return 2;
    }

    close(sock);

    lua_pushstring(L, data);

    return 1;
}

static const luaL_Reg regs[] = {
    {"set", lua_iwpriv_set},
    {"show", lua_iwpriv_show},
    {"get_site_survey2", lua_iwpriv_get_site_survey2},
    {NULL, NULL}
};

int luaopen_iwpriv(lua_State *L)
{
    luaL_newlib(L, regs);

    return 1;
}
