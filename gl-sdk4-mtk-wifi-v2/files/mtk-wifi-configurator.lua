#!/usr/bin/lua

local iwpriv = require 'iwpriv'
local uloop = require 'uloop'
local ubus = require 'ubus'
local uci = require 'uci'

local device_configs = {}
local ap_configs = {}
local setup_pending = {}
local teardown_pending = {}
local teardown_tmr
local setting = false

-- The available formats are:
-- "n": reads a numeral and returns it as a float or an integer, following the lexical conventions of Lua.
-- "a": reads the whole file. This is the default format.
-- "l": reads the next line skipping the end of line.
-- "L": reads the next line keeping the end-of-line character (if present).
-- number: reads a string with up to this number of bytes. If number is zero, it reads nothing and returns an empty string.
-- Return nil if the file open failed
local function readfile(name, format)
    local f = io.open(name, "r")
    if not f then return nil end

    -- Compatible with the version below 5.3
    if type(format) == "string" and format:sub(1, 1) ~= "*" then format = "*" .. format end

    local data

    if format == "*L" and tonumber(_VERSION:match("%d.%d")) < 5.2 then
        data = f:read("*l")
        if data then data = data .. "\n" end
    else
        data = f:read(format or "*a")
    end

    f:close()
    return data or ""
end

local function writefile(name, data, append)
    local f = io.open(name, append and "a" or "w")
    if not f then return nil end
    f:write(data)
    f:close()
    return true
end

local function log(...)
    local msg = table.concat({...})
    local f = io.open('/var/log/mtk-wifi', 'a')
    if not f then
        return
    end

    f:write(os.date() .. ' ')
    f:write(msg)
    f:write('\n')
    f:close()
end

local function ifup(ifname)
    os.execute('ifconfig ' .. ifname .. ' up')
end

local function ifdown(ifname)
    os.execute('ifconfig ' .. ifname .. ' down')
end

local function file_exist(path)
    local f = io.open(path)
    if f then
        f:close()
    end
    return f ~= nil
end

local function iwpriv_set(ifname, key, val)
    iwpriv.set(ifname, key, val)
    log('iwpriv ', ifname, ' set ', key, '=', val)
end

local function netdev_is_up(ifname)
    local path = '/sys/class/net/' .. ifname
    return file_exist(path) and readfile(path .. '/operstate', '*l') ~= 'down'
end

local function down_vif(ifname)
    if netdev_is_up(ifname) then
        log('down ', ifname)
        ifdown(ifname)
        os.execute('ip link set dev ' .. ifname .. ' nomaster')
        ubus.call('service', 'delete', { name = ifname .. '-8021xd' })
    end
end

local function setup_bssid_num()
    local c = uci.cursor()

    local devices = {}

    c:foreach('wireless', 'wifi-iface', function(s)
        if s.mode == 'ap' then
            if not devices[s.device] then
                devices[s.device] = 1
            else
                devices[s.device] = devices[s.device] + 1
            end
        end
    end)

    local need_restart = false

    for device, num in pairs(devices) do
        local path

        if device == 'mt798111' then
            path = '/etc/wireless/mediatek/mt7981.dbdc.b0.dat'
        elseif device == 'mt798112' then
            path = '/etc/wireless/mediatek/mt7981.dbdc.b1.dat'
        end

        local lines = {}

        for line in io.lines(path) do
            if line:match('BssidNum') then
                local old = tonumber(line:match('(%d+)'))
                if old ~= num then
                    line = 'BssidNum=' .. num
                    need_restart = true
                end
            end

            lines[#lines + 1] = line
        end

        writefile(path, table.concat(lines, '\n') .. '\n')
    end

    if need_restart then
        for i = 0, 4 do
            down_vif('ra' .. i)
            down_vif('rax' .. i)
        end
    end
end

local function device_to_main_dev(dev)
    if dev == 'mt798111' then
        return 'ra0'
    elseif dev == 'mt798112' then
        return 'rax0'
    end
end

local function device_to_apcli(dev)
    if dev == 'mt798111' then
        return 'apcli0'
    elseif dev == 'mt798112' then
        return 'apclix0'
    end
end

local function uci_encryption_to_mtk(encryption)
    local AuthMode = 'OPEN'
    local EncrypType = 'NONE'
    local is_wpa = false

    if encryption:match('^psk') or encryption:match('^sae') or encryption:match('^wpa') then
        if encryption:match('^sae') then
            AuthMode = 'WPA3PSK'
        elseif encryption:match('^psk2') then
            AuthMode = 'WPA2PSK'
        elseif encryption:match('^psk') then
            AuthMode = 'WPAPSK'
        elseif encryption:match('^wpa3') then
            AuthMode = 'WPA3'
        elseif encryption:match('^wpa2') then
            AuthMode = 'WPA2'
        else
            AuthMode = 'WPA'
        end

        if encryption:match('mixed') then
            if encryption:match('^sae') then
                AuthMode = 'WPA2PSKWPA3PSK'
            elseif encryption:match('^psk') then
                AuthMode = 'WPAPSKWPA2PSK'
            elseif encryption:match('^wpa') then
                AuthMode = 'WPA1WPA2'
            end
        end

        local ciphers = {}

        EncrypType = ''

        if encryption:match('tkip') then
            ciphers['TKIP'] = true
        end

        if encryption:match('aes') or encryption:match('ccmp') then
            ciphers['AES'] = true
        end

        if ciphers['TKIP'] then
            EncrypType = 'TKIP'
        end

        if ciphers['AES'] then
            EncrypType = EncrypType .. 'AES'
        end

        if EncrypType == '' then
            EncrypType = 'AES'
        end

        if encryption:match('^wpa') then
            is_wpa = true
        end
    end

    return AuthMode, EncrypType, is_wpa
end

local function setup_vif(device, name, ifs)
    local cfg = ifs.config
    local ifname = cfg.ifname

    if cfg.mode == 'ap' then
        if not ifname then
            log('not found ifname for ifs:', name)
            return
        end
    elseif cfg.mode == 'sta' then
        if not ifname then
            ifname = device_to_apcli(device)
            local c = uci.cursor()
            c:foreach('wireless', 'wifi-iface', function(s)
                if s.device == device and s.mode == 'sta' then
                    c:set('wireless', s['.name'], 'ifname', ifname)
                end
            end)
            c:commit('wireless')
            return 'reload'
        end
    else
        log('not supported mode: ', cfg.mode)
        return
    end

    local old = ap_configs[ifname] or {}
    local need_set_ssid = false

    log('setup iface: ', ifname)
    ifup(ifname)

    ubus.call('network.wireless', 'notify', {
        command = 1,
        device = device,
        interface = name,
        data = {
            ifname = ifname
        }
    })

    if cfg.mode == 'ap' then
        cfg.wmm = cfg.wmm or true
        if cfg.wmm ~= old.wmm then
            iwpriv_set(ifname, 'WmmCapable', cfg.wmm and 0 or 1)
        end

        cfg.isolate = cfg.isolate or false
        if cfg.isolate ~= old.isolate then
            iwpriv_set(ifname, 'NoForwarding', cfg.isolate and 1 or 0)
        end

        cfg.hidden = cfg.hidden or false
        if cfg.hidden ~= old.hidden then
            iwpriv_set(ifname, 'HideSSID', cfg.hidden and 1 or 0)
        end

        cfg.ieee80211k = cfg.ieee80211k or false
        if cfg.ieee80211k ~= old.ieee80211k then
            iwpriv_set(ifname, 'RRMEnable', cfg.ieee80211k and 1 or 0)
        end

        cfg.maxassoc = cfg.maxassoc or 0
        if cfg.maxassoc ~= old.maxassoc then
            iwpriv_set(ifname, 'MbssMaxStaNum', cfg.maxassoc or 0)
        end

        cfg.rsn_preauth = cfg.rsn_preauth or false
        if cfg.rsn_preauth ~= old.rsn_preauth then
            iwpriv_set(ifname, 'PreAuth', cfg.rsn_preauth and 1 or 0)
        end

        cfg.ieee80211w = cfg.ieee80211w or 0
        if cfg.ieee80211w ~= old.ieee80211w then
            local PMFMFPC = '0'
            local PMFMFPR = '0'

            if cfg.ieee80211w == 1 then
                PMFMFPC = '1'
                PMFMFPR = '0'
            elseif cfg.ieee80211w == 2 then
                PMFMFPC = '1'
                PMFMFPR = '1'
            end

            iwpriv_set(ifname, 'PMFMFPC', PMFMFPC)
            iwpriv_set(ifname, 'PMFMFPR', PMFMFPR)
        end

        if cfg.encryption ~= old.encryption or cfg.key ~= old.key then
            local AuthMode, EncrypType, is_wpa = uci_encryption_to_mtk(cfg.encryption)

            if is_wpa then
                iwpriv.set(ifname, 'RADIUS_Key', cfg.key)
                ubus.call('service', 'add', {
                    name = ifname .. '-8021xd',
                    instances = {
                        instance1 = {
                            command = {
                                '/usr/bin/8021xd',
                                '-p',
                                ifname:match('%a+'),
                                '-i',
                                ifname
                            }
                        }
                    }
                })
            else
                iwpriv.set(ifname, 'WPAPSK', cfg.key)
                ubus.call('service', 'delete', { name = ifname .. '-8021xd' })
            end

            if AuthMode == 'OPEN' then
                iwpriv_set(ifname, 'RekeyMethod', 'DISABLE')
            else
                iwpriv_set(ifname, 'RekeyMethod', 'TIME')
            end

            iwpriv_set(ifname, 'AuthMode', AuthMode)
            iwpriv_set(ifname, 'EncrypType', EncrypType)

            need_set_ssid = true
        end

        if cfg.server ~= old.server then
            iwpriv_set(ifname, 'RADIUS_Server', cfg.server or '')
        end

        if cfg.port ~= old.port then
            iwpriv_set(ifname, 'RADIUS_Port', cfg.port or 1812)
        end

        if cfg.ssid ~= old.ssid or need_set_ssid then
            iwpriv_set(ifname, 'ssid', cfg.ssid)
        end

        if ifs.bridge then
            os.execute('ip link set dev ' .. ifname .. ' master ' .. ifs.bridge)
        end
    else
        if cfg.macaddr ~= old.macaddr then
            ifdown(ifname)
            os.execute('ip link set ' .. ifname .. ' address' .. (cfg.macaddr or '00:00:00:00:00:00'))
            ifup(ifname)
        end

        if cfg.ssid ~= old.ssid then
            iwpriv_set(ifname, 'ApCliSsid', cfg.ssid)
        end

        if cfg.bssid ~= old.bssid then
            iwpriv_set(ifname, 'ApCliBssid', cfg.bssid)
        end

        if cfg.encryption ~= old.encryption then
            local AuthMode, EncrypType, is_wpa = uci_encryption_to_mtk(cfg.encryption)
            if is_wpa then
                log('not support eap for sta')
            else
                iwpriv_set(ifname, 'ApCliAuthMode', AuthMode)
                iwpriv_set(ifname, 'ApCliEncrypType', EncrypType)
            end
        end

        if cfg.key ~= old.key then
            iwpriv_set(ifname, 'ApCliWPAPSK', cfg.key)
        end

        iwpriv_set(ifname, 'ApCliPMFMFPC', 1)
        iwpriv_set(ifname, 'ApCliDelPMKIDList', 1)

        iwpriv_set(ifname, 'ApCliEnable', 1)

        local network = cfg.network[1]
        if network then
            ubus.call('network.interface.' .. network, 'add_device', { name = ifname })
        end
    end

    ap_configs[ifname] = cfg
end

local function setup_vifs(device, interfaces)
    local reload = false
    for name, ifs in pairs(interfaces) do
        if setup_vif(device, name, ifs) == 'reload' then
            reload = true
        end
    end

    if reload then
        return 'reload'
    end
end

local function down_disabled_vif()
    local c = uci.cursor()

    local uped = {}

    c:foreach('wireless', 'wifi-iface', function(s)
        if s.disabled ~= '1' then
            uped[s.ifname] = true
        end
    end)

    for _, prefix in ipairs({ 'ra', 'rax', 'apcli', 'apclix' }) do
        for i = 0, 4 do
            local ifname = prefix .. i
            if not uped[ifname] then
                down_vif(ifname)
            end
        end
    end
end

local country_region_map = {
    JO = 10,
    TZ = 4, CV = 4, BD = 4, CU = 4, BZ = 4, IR = 4,
    HK = 7, HM = 7, KP = 7, LC = 7, KN = 7, NC = 7, NA = 7, MY = 7, NF = 7, NE = 7, MZ = 7,
    ['00'] = 13, EC = 13, PS = 13, LK = 13, US = 13, CK = 13, AS = 13, TC = 13, BS = 13, TK = 13, GH = 13, MH = 13, MX = 13, VU = 13, VN = 13, VI = 13, CX = 13, KR = 13, LS = 13, PH = 13, UG = 13, UA = 13, SH = 13, GI = 13, KY = 13, CI = 13, GY = 13, LR = 13, TJ = 13, NI = 13, TH = 13, CF = 13, TV = 13, CG = 13, PY = 13, SN = 13, MN = 13, RW = 13, FO = 13, CO = 13, TW = 13, SG = 13, PG = 13, PW = 13, GW = 13, SO = 13, MO = 13, PA = 13, LB = 13, NZ = 13, MP = 13, GD = 13, TT = 13, MU = 13, TL = 13, HT = 13, FM = 13, BM = 13, PE = 13, UM = 13, TM = 13, GU = 13, LA = 13, PR = 13, GB = 13, KZ = 13, JM = 13, AE = 13, HN = 13, AR = 13, BR = 13, CR = 13, FK = 13, MK = 13, DK = 13, ES = 13, GS = 13, PK = 13, IS = 13, ME = 13, AD = 13, CH = 13, IN = 13, BA = 13, FI = 13, GA = 13, LI = 13, RS = 13, LY = 13, CY = 13, QA = 13, IQ = 13, MD = 13, LV = 13, BF = 13, LU = 13, SL = 13, SK = 13, SJ = 13, SI = 13, IO = 13, BG = 13, MC = 13, RO = 13, NO = 13, IL = 13, PT = 13, LT = 13, AL = 13, AT = 13, MT = 13, IT = 13, PL = 13, ML = 13, NL = 13, EE = 13, DE = 13, IM = 13, MM = 13, JE = 13, IE = 13, SE = 13, SM = 13, HU = 13, FJ = 13, BE = 13, DJ = 13, CZ = 13, FR = 13, GR = 13, HR = 13,
    CC = 14, CA = 14, CD = 14, AU = 14, RU = 14,
    EH = 2, VA = 2, TN = 2, EG = 2, KW = 2, TO = 2, AM = 2, GE = 2, MA = 2, UZ = 2, ER = 2, AZ = 2,
    MS = 1, WS = 1, SC = 1, ZW = 1, YT = 1, KH = 1, WF = 1, MF = 1, VC = 1, GP = 1, JP = 1, AX = 1, KI = 1, AI = 1, SA = 1, BY = 1, MQ = 1, GQ = 1, ZA = 1, AF = 1, AN = 1, GF = 1, ST = 1, SR = 1, TF = 1, BV = 1, PF = 1, GN = 1, PN = 1, AG = 1, GG = 1, AO = 1, TG = 1, BW = 1, AW = 1, GL = 1, BL = 1, BT = 1, SD = 1, TD = 1, MW = 1, ET = 1, GM = 1, MG = 1, ZM = 1, RE = 1, PM = 1, OM = 1, MR = 1, KM = 1, TR = 1, SB = 1, AQ = 1, DZ = 1,
    NP = 0, BH = 0, VG = 0, UY = 0, SV = 0, BI = 0, BN = 0, CN = 0, DO = 0, NR = 0, NU = 0, CL = 0, MV = 0, GT = 0, DM = 0, CM = 0, VE = 0, BB = 0, BJ = 0, ID = 0,
    BO = 3, NG = 3,
    KG = 11, KE = 11
}

local setup_tmr = uloop.timer(function()
    setup_bssid_num()

    local reload = false

    for device, cfg in pairs(setup_pending) do
        if cfg.interface_cnt > 0 then
            log('setup: ', device)

            local ifname = device_to_main_dev(device)

            if not file_exist('/sys/class/net/' .. ifname) then
                ifup('ra0')
            end

            ifup(ifname)

            if not device_configs[device] then
                device_configs[device] = {}
            end

            local old = device_configs[device]

            local htmode, bw = cfg.htmode:match('(%a+)(%d+)')
            local require_mode = cfg.require_mode

            local WirelessMode
            local VhtDisallowNonVHT
            local HtBssCoex
            local HtBw
            local VhtBw

            if cfg.band == '2g' then
                if htmode == 'HE' then
                    WirelessMode = 16
                elseif htmode == 'HT' then
                    WirelessMode = 9
                else
                    WirelessMode = 4
                end
            else
                if htmode == 'HE' then
                    WirelessMode = 17
                elseif htmode == 'VHT' then
                    WirelessMode = 14
                elseif htmode == 'HT' then
                    WirelessMode = 8
                else
                    WirelessMode = 2
                end

                if require_mode == 'ac' then
                    VhtDisallowNonVHT = 1
                else
                    VhtDisallowNonVHT = 0
                end
            end

            if bw == '20' then
                HtBw = 0
                VhtBw = 0
            elseif bw == '40' then
                HtBw = 1
                VhtBw = 0

                if cfg.noscan == '1' then
                    HtBssCoex = 0
                else
                    HtBssCoex = 1
                end
            elseif bw == '80' then
                HtBw = 1
                VhtBw = 1
            elseif bw == '160' then
                HtBw = 1
                VhtBw = 2
            end

            if WirelessMode ~= old.WirelessMode then
                iwpriv_set(ifname, 'WirelessMode', WirelessMode)
                old.WirelessMode = WirelessMode
            end

            if VhtDisallowNonVHT ~= old.VhtDisallowNonVHT then
                iwpriv_set(ifname, 'VhtDisallowNonVHT', VhtDisallowNonVHT)
                old.VhtDisallowNonVHT = VhtDisallowNonVHT
            end

            local need_acs = false

            if HtBw ~= old.HtBw then
                iwpriv_set(ifname, 'HtBw', HtBw)
                old.HtBw = HtBw
                need_acs = true
            end

            if VhtBw ~= old.VhtBw then
                iwpriv_set(ifname, 'VhtBw', VhtBw)
                old.VhtBw = VhtBw
                need_acs = true
            end

            if HtBssCoex ~= old.HtBssCoex then
                iwpriv_set(ifname, 'HtBssCoex', HtBssCoex)
                old.HtBssCoex = HtBssCoex
            end

            if cfg.txpower ~= old.txpower then
                local txpower = cfg.txpower
                local PercentageCtrl = txpower < 100 and 1 or 0
                iwpriv_set(ifname, 'txpower', txpower)
                iwpriv_set(ifname, 'PercentageCtrl', PercentageCtrl)
                old.txpower = cfg.txpower
            end

            local country = cfg.country or '00'
            if cfg.band == '5g' then
                local region = country_region_map[country] or 13
                if region ~= old.region then
                    iwpriv_set(ifname, 'CountryRegionABand', region)
                    old.region = region
                    need_acs = true
                end
            else
                local region = 0
                if country == 'JP' then
                    region = 1
                end

                if region ~= old.region then
                    iwpriv_set(ifname, 'CountryRegion', region)
                    old.region = region
                    need_acs = true
                end
            end

            if country ~= old.country then
                iwpriv_set(ifname, 'CountryCode', country)
                old.country = country
            end

            if cfg.channel ~= old.channel then
                local channel = cfg.channel

                if channel == 'auto' then
                    iwpriv_set(ifname, 'AutoChannelSel', 3)
                else
                    iwpriv_set(ifname, 'channel', cfg.channel)
                end

                old.channel = cfg.channel
            elseif need_acs and cfg.channel == 'auto' then
                iwpriv_set(ifname, 'AutoChannelSel', 3)
            end

            if setup_vifs(device, cfg.interfaces) == 'reload' then
                reload = true
            end
        end
    end

    down_disabled_vif()

    setting = false

    if reload then
        ubus.call('network', 'reload')
    end
end)

local function teardown_cb()
    for device in pairs(teardown_pending) do
        log('teardown: ', device)

        local ifname = device_to_main_dev(device)
        local ifname_prefix = ifname:match('%a+')

        for i = 0, 4 do
            down_vif(ifname_prefix .. i)
        end

        down_vif(device_to_apcli(device))
    end
end

local function main()
    uloop.init()

    local ubus_conn = ubus.connect()

    ubus.ubus_conn = ubus_conn

    ubus.call = function(object, method, params)
        return ubus_conn:call(object, method, params or {})
    end

    ubus.reply = function(req, msg)
        return ubus_conn:reply(req, msg or {})
    end

    ubus_conn:add({
        ['mtk-wifi'] = {
            setup = {
                function (req, msg)
                    os.remove('/var/log/mtk-wifi')

                    local device = msg.device
                    local cfg = msg.config

                    if not device or not cfg then
                        return
                    end

                    local interface_cnt = 0

                    for _ in pairs(msg.interfaces or {}) do
                        interface_cnt = interface_cnt + 1
                    end

                    cfg.interfaces = msg.interfaces
                    cfg.interface_cnt = interface_cnt

                    setup_pending[device] = cfg
                    setup_tmr:set(100)

                    if teardown_tmr then
                        teardown_tmr:cancel()
                        teardown_tmr = nil
                    end

                    setting = true
                end, {}
            },
            teardown = {
                function (req, msg)
                    os.remove('/var/log/mtk-wifi')

                    local device = msg.device

                    if not device then
                        return
                    end

                    teardown_pending[device] = true

                    if not teardown_tmr then
                        teardown_tmr = uloop.timer(teardown_cb)
                    end
                    teardown_tmr:set(1000)
                end, {}
            },
            status = {
                function (req, msg)
                    ubus.reply(req, { up = not setting })
                end, {}
            }
        }
    })

    uloop.run()
end

local ok, err = pcall(main)
if not ok then
    if not err then
        err = 'unknown panic'
    end

    log(err)
end
