local fs = require "oui.fs"
local uci = require 'uci'
local rpc = require 'oui.rpc'
local bit = require 'bit'
local cjson = require "cjson"
local otUtils = require 'otbr.utils'
local otUci = require 'otbr.uci'
local otUbus = require 'otbr.ubus'
local otSrp = require 'otbr.srp'
local otBbr = require 'otbr.bbr'
local otSrp = require 'otbr.srp'

local M = {}

function split(str, reps)
    local resultStrList = {}
    string.gsub(str, '[^' .. reps .. ']+', function(w)
        table.insert(resultStrList, w:match('^[%s]*(.-)[%s]*$'))
    end)
    return resultStrList
end

-- Thread Network
function M.generate_thread_network()
    local channel, networkname, passphrase

    if fs.access('/tmp/modify_flag') then os.execute('rm /tmp/modify_flag') end

    -- channel = otUci.Get('otbr', "otbr", "channel")
    networkname = otUci.Get('otbr', "otbr", "networkname")

    os.execute('ot-ctl dataset init new')
    -- if channel ~= nil then os.execute('ot-ctl dataset channel ' .. channel) end
    if networkname ~= nil then
        os.execute('ot-ctl dataset networkname ' .. networkname)
    end
    if passphrase ~= nil then
        os.execute('ot-ctl dataset pskc -p ' .. passphrase)
    end
    os.execute('ot-ctl dataset commit active')
    
    otUci.Set('otbr', 'otbr', "activetimestamp", "1")

    return {err_code = 0}
end

function M.export_thread_network()
    local f
    local dataset
    local passphrase
    local exportData

    passphrase = otUci.Get('otbr', "otbr", "passphrase")

    dataset = otUtils.GetDatasetActive()
    if passphrase ~= nil then
        exportData = passphrase .. '|' .. dataset
    else
        exportData = dataset
    end

    f = io.open('/tmp/thread_network.conf', 'w')
    f:write(exportData)
    io.close(f)
    return {
        filename = "thread_network.conf",
        path = "/tmp/thread_network.conf"
    }
end

function M.import_thread_network(params)
    local passphrase = params.Passphrase
    local dataset = params.ActiveDataset
    local importData
    local err_code = 0

    if dataset == nil then
        local f
        f = io.open('/tmp/thread_network.conf', 'r')
        if f ~= nil then
            importData = f:read()
            io.close(f)
        
            if string.find(importData, '|') ~= nil then
                passphrase = string.sub(importData, 1, string.find(importData, '|') - 1)
                dataset = string.sub(importData, string.find(importData, '|') + 1, -1)
            else
                dataset = importData
            end
        end
    end

    err_code = otUtils.SetDatasetActive(dataset)
    if (err_code == 0) then
        os.execute('sleep 2')
        local networkname = otUtils.GetNetworkName()
        local channel = otUtils.GetChannel()
        otUci.Set('otbr', "otbr", "enable", "1")
        otUci.Set('otbr', "otbr", "networkname", networkname)
        otUci.Set('otbr', "otbr", "channel", channel)
        otUci.Set('otbr', "otbr", "passphrase", passphrase)
        otUtils.ThreadStart()
    end

    return {err_code = err_code}
end

function M.get_status()
    if (otUtils.isRunning() ~= 0) then return rpc.ERROR_CODE_INTERNAL_ERROR end

    local data = {}
    data = otUtils.GetStatus()
    data.Error = nil
    data.RCP.TXPower = otUtils.GetTxPower()

    return data
end

function M.scan() 
    return otUtils.GetScanLists() 
end

function M.set_config(params)
    local NetworkName = params.NetworkName
    local Passphrase = params.Passphrase
    local PanId = params.PanId
    local ExtPanId = params.ExtPanId
    local NetworkKey = params.NetworkKey
    local Channel = params.Channel
    local Prefix = params.OnMeshPrefix
    local err_code = 0

    if (otUtils.GetState() == 'disabled') then
        if (otUtils.SetNetworkName(NetworkName) ~= 0) then
            return {err_code = -1, err_msg = 'Invalid NetworkName'}
        end

        if (otUtils.SetChannel(Channel) ~= 0) then
            return {err_code = -1, err_msg = 'Invalid Channel'}
        end

        if (otUtils.SetPanId(PanId) ~= 0) then
            return {err_code = -1, err_msg = 'Invalid PanId'}
        end

        if (otUtils.SetExtPanId(ExtPanId) ~= 0) then
            return {err_code = -1, err_msg = 'Invalid ExtPanId'}
        end

        if (otUtils.SetNetworkKey(NetworkKey) ~= 0) then
            return {err_code = -1, err_msg = 'Invalid NetworkKey'}
        end

        if (otUtils.SetPSKc(Passphrase) ~= 0) then
            return {err_code = -1, err_msg = 'Invalid Passphrase'}
        end
        otUtils.ThreadStart()
    else
        err_code = otUtils.MgmtSet(NetworkKey, NetworkName, ExtPanId, PanId, tostring(Channel), Passphrase)
        os.execute('echo 1 > /tmp/modify_flag')
    end

    -- if (otUtils.SetPrefix(Prefix) ~= 0) then
    --     return {err_code = 1, err_msg = 'Invalid Prefix'}
    -- end

    return {err_code = err_code}
end

function M.set_txpower(params)
    local txpower = params.TXPower
    local err_code

    err_code = otUtils.SetTxPower(txpower)

    return {err_code = err_code}
end

local function SetJoinerTimeout()
    local JoiningDevices = otUtils.GetJoiningDevices()
    for k, v in pairs(JoiningDevices) do
        if v.eui64 ~= nil then
            otUci.Del('thread_devices', v.eui64, "status")
        end
    end
end

function M.stop()
    SetJoinerTimeout()

    otUtils.ThreadStop()

    return {err_code = 0}
end

function M.start()
    otUtils.ThreadStart()
    return {err_code = 0}
end

function M.join(params)
    local res
    local CredentialType = params.CredentialType
    local NetworkKey = params.NetworkKey
    local Channel = params.Channel
    local PanId = params.PanId
    local NetworkName = params.NetworkName
    local PSKd = params.PSKd

    otUtils.ThreadStop()
    
    os.execute("ubus send otbr-agent '{\"id\":6}'")
    if CredentialType == 'networkKeyType' then
        res = otUtils.ThreadJoinStartWithNetworkKey(PanId, Channel, NetworkKey,
                                               NetworkName)
    else
        res = otUtils.ThreadJoinStartWithPSKd(PSKd)
    end

    return {err_code = res}
end

-- Thread Devices
local function set_timeout()
    local c = uci.cursor()
    c:foreach('thread_devices', 'device', function(s)
        if s.Joined == "0" and s.status == "0" then
            c:set('thread_devices', s.EUI64, 'status', '1')
        end
    end)
    c:commit('thread_devices')
end

function M.add_joiner(params)
    local PSKd = params.PSKd
    local Timeout = params.Timeout
    local EUI64 = params.EUI64
    local err_code = 0

    if (EUI64 ~= nil) then
        EUI64 = string.lower(EUI64)
    end

    if EUI64 ~= '*' then
        otUtils.CommissionerSaveJoinerInfo(PSKd, EUI64, Timeout)
    end

    if otUtils.GetCommissionerState() == 2 then
        err_code = otUtils.CommissionerAddJoiner(PSKd, EUI64, Timeout)
    end

    return {err_code = 0}
end

function M.export_joiner_list()
    local c = uci.cursor()
    local f
    local err_code = 0

    f = io.open('/tmp/thread_devices.csv', 'w')
    f:write('Joiner EUI64,Joiner Credential\n')
    c:foreach('thread_devices', 'device', function(s)
        if s.EUI64 ~= nil and s.PSKd ~= nil then
            f:write(s.EUI64 .. ',' .. s.PSKd .. '\n')
        end
    end)
    io.close(f)

    return {err_code = err_code}
end

function M.import_joiner_list(params)
    local err_code = 0
    local Timeout = params.Timeout

    if Timeout == nil then return {err_code = -2, err_msg = "Invalid parameter"} end

    local State = otUtils.GetCommissionerState()

    local f
    f = io.open('/tmp/thread_devices', 'r')
    if f ~= nil then
        for line in f:lines() do
            data = split(line, ',')
            if data[1] ~= 'Joiner EUI64' then
                EUI64 = string.lower(data[1])
                PSKd = data[2]
                otUtils.CommissionerSaveJoinerInfo(PSKd, EUI64, Timeout)
                if State == 2 then
                    otUtils.CommissionerAddJoiner(PSKd, EUI64, Timeout)
                    os.execute('sleep 0.03')
                end
            end
        end
    end

    return {err_code = err_code}
end

function M.rejoin_all(params)
    local err_code = 0
    local Timeout = params.Timeout
    local Type = params.Type
    local c = uci.cursor()

    -- if otUtils.GetCommissionerState() ~= 2 then
    --     return {err_code = -1, err_msg = "Commissioning active failed"}
    -- end

    c:foreach('thread_devices', 'device', function(s)
        if Type == 1 then
            if s.PSKd ~= nil and s.EUI64 ~= nil and s.Joined == '0' and s.status == nil then
                otUtils.CommissionerAddJoiner(s.PSKd, s.EUI64, Timeout)
                otUtils.CommissionerSaveJoinerInfo(s.PSKd, s.EUI64, Timeout)
                os.execute('sleep 0.03')
            end
        elseif Type == 2 then
            if s.PSKd ~= nil and s.EUI64 ~= nil and s.Joined == '1' then
                otUtils.CommissionerAddJoiner(s.PSKd, s.EUI64, Timeout)
                otUtils.CommissionerSaveJoinerInfo(s.PSKd, s.EUI64, Timeout)
                os.execute('sleep 0.03')
            end
        elseif Type == 3 then
            if s.PSKd ~= nil and s.EUI64 ~= nil and s.status == nil then
                otUtils.CommissionerAddJoiner(s.PSKd, s.EUI64, Timeout)
                otUtils.CommissionerSaveJoinerInfo(s.PSKd, s.EUI64, Timeout)
                os.execute('sleep 0.03')
            end
        else
            return {err_code = -2, err_msg = "Invalied parameter"}
        end
    end)

    return {err_code = err_code}
end

function M.remove_joiner_list(params)
    local err_code
    local c = uci.cursor()
    local DeviceList = params.DeviceList

    for k, v in pairs(DeviceList) do
        c:delete('thread_devices', v)
        otUtils.CommissionerRemoveJoiner(v)
    end
    c:commit('thread_devices')

    return {err_code = 0}
end

-- function M.modify_joiner(params)
--     local EUI64 = string.lower(params.EUI64)
--     local Name = params.Name

--     err_code = otUtils.CommissionerRenameJoiner(EUI64, Name)
--     if err_code ~= 0 then return rpc.ERROR_CODE_INVALID_PARAMS end

--     return {err_code = 0}
-- end

local function joining(eui64, JoiningDevices)
    for k, v in pairs(JoiningDevices) do
        if eui64 == v.eui64 then
            return true
        end
    end
    return false
end

function M.get_joiner_list()
    local data = {}
    local c = uci.cursor()
    local JoiningDevices = otUtils.GetJoiningDevices()

    data.DeviceList = {}

    c:foreach('thread_devices', 'device', function(s)
        for k, v in pairs(s) do if k:sub(1, 1) == '.' then s[k] = nil end end
        if s.PSKd ~= nil then
            if s.Joined == '1' then
                s.Status = 2            -- Joined
            elseif joining(s.EUI64, JoiningDevices) then
                s.Status = 1            -- Joining
            else
                if s.status == '0' then
                    s.Status = 0        -- Ready to Join
                else
                    s.Status = 3        -- Timeout
                end
            end
            s.status = nil
            s.Joined = nil
            if s.Timeout ~= nil then s.Timeout = tonumber(s.Timeout) end
            if s.AddTime ~= nil then s.AddTime = tonumber(s.AddTime) end
            data.DeviceList[#data.DeviceList + 1] = s
        end
    end)

    return data
end

-- Thread Topologies
local function GetGLDeviceName(rloc16)
    local res, name
    local conn = ubus.connect()

    res = conn:call('otbr-gw', 'get_device_status', {rloc16 = rloc16})
    conn:close()

    if res == nil then return "" end

    if res.err_code ~= 0 then 
        name = ""
    else
        name = res.dev_name
    end

    return name
end

function M.get_network_data()
    if (otUtils.isRunning() ~= 0) then return rpc.ERROR_CODE_INTERNAL_ERROR end

    local NetworkDataList = otUtils.GetNetworkData().NetworkDataList
    for k, v in pairs(NetworkDataList) do
        v.Name = GetGLDeviceName(v.Rloc16)
        if v.ChildTable == nil then
            v.ChildTable = {}
        else
            for m, n in pairs(v.ChildTable) do
                n.Name = GetGLDeviceName(v.Rloc16 + n.ChildId)
            end
        end
    end

    return NetworkDataList
end

function M.export_network_data()
    local state = otUtils.GetState()
    if (state == 'disabled' or state == 'detached') then
        return {err_code = 1}
    end

    local data = {}
    local http = require "resty.http"
    local httpc = http.new()
    local res, err = httpc:request_uri("http://127.0.0.1:8081/diagnostics", {
        method = "GET",
        headers = {["Content-Type"] = "application/json"}
    })
    if not res then
        ngx.log(ngx.ERR, "request failed: ", err)
        return {err_code = 1}
    end

    -- At this point, the entire request / response is complete and the connection
    -- will be closed or back on the connection pool.
    -- The `res` table contains the expeected `status`, `headers` and `body` fields.
    local status = res.status
    local length = res.headers["Content-Length"]
    local body = res.body

    local f
    f = io.open('/tmp/thread_network_diagnostics.json', 'w')
    f:write(body)
    io.close(f)
    
    os.execute("rm /www/js/thread_network_diagnostics.json; ln -sf /tmp/thread_network_diagnostics.json /www/js/thread_network_diagnostics.json")

    return { file_name = "thread_network_diagnostics.json", file_path = "/js/thread_network_diagnostics.json"}
end

function M.get_neighbor_list()
    if (otUtils.isRunning() ~= 0) then return rpc.ERROR_CODE_INTERNAL_ERROR end

    local res = {}
    res = otUtils.GetNeighborList()
    res.Error = nil

    return res
end

-- Backbone Routers
function M.get_bbr_status() return otBbr.BackboneRouterStatus() end

function M.enable_bbr()
    local err_code = 0
    err_code = otBbr.BackboneRouterStart()
    return {err_code = err_code}
end

function M.disable_bbr()
    local err_code = 0
    err_code = otBbr.BackboneRouterStop()
    return {err_code = err_code}
end

function M.set_bbr_config(params)
    local err_code = 0
    local Enable = params.Enable
    local MlrTimeout = params.MlrTimeout
    local SequenceNumber = params.SequenceNumber
    local ReregistrationDelay = params.ReregistrationDelay
    local Jitter = params.Jitter
    local IfName = params.IfName

    err_code = otBbr.BackboneRouterConfig(Enable, MlrTimeout, SequenceNumber,
                                       ReregistrationDelay, Jitter, IfName)

    return {err_code = err_code}
end

function M.get_commissioning_status()
    local data = {}

    State = otUbus.Call('otbr', 'commissionerstate').State

    data.State = State

    return data
end

function M.set_commissioning(params)
    local Enable = params.Enable
    local err_code
    if Enable then
        err_code = otUtils.CommissionerStart()
        if err_code == 0 then
            os.execute('sleep 1')
            
            local c = uci.cursor()
            c:foreach('thread_devices', 'device', function(s)
                if s.status == '0' and s.Joined == '0' then
                    otUtils.CommissionerAddJoiner(s.PSKd, s.EUI64, s.Timeout)
                    c:set('thread_devices', s.EUI64, 'status', '1') -- Joining
                    os.execute('sleep 0.03')
                end
            end)
            c:commit('thread_devices')
        else
            err_msg = "Commissioning active failed"
        end
    else
        err_code = otUtils.CommissionerStop()
    end

    return {err_code = err_code, err_msg = err_msg}
end

function M.get_srp_server_service()
    return otSrp.GetSrpServerServices()
end

function M.get_srp_server_config()
    return otSrp.GetSrpServerConfig()
end

function M.set_srp_server_config(params)
    local res, err_code
    err_code = otSrp.SetSrpServerConfig(params)

    return {err_code = err_code} 
end

return M
