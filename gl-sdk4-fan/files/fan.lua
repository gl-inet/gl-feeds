--[[
    @object-name: fan
    @object-desc: This is the API related to fan Internet access.
--]]

local uci = require "uci"
local utils = require "oui.utils"
local fs = require "oui.fs"

local M = {}

local function stop_test_fan()
    utils.writefile("/sys/class/thermal/cooling_device0/cur_state", "0")
    ngx.pipe.spawn({"/etc/init.d/gl_fan", "restart"}):wait()
end

--[[
    @method-name: set_test
    @method-desc: Set fan test.

    @in bool     test             测试风扇起转.
    @in number   time             测试风扇起转时间s, 缺省为10s.
    @in-example:  {\"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":[\"\",\"fan\",\"set_test\",{\"test\":true,\"time\":5}],\"id\":1}
    @out-example: {\"jsonrpc\": \"2.0\", \"id\": 1, \"result\": {}}
--]]

M.set_test = function(params)

    local test = params.test
    if test == true then
        local time = params.time or 10
        ngx.pipe.spawn({"/etc/init.d/gl_fan", "stop"}):wait()
        utils.writefile("/sys/class/thermal/cooling_device0/cur_state", "255")
        ngx.timer.at(time, stop_test_fan)
    end
end

--[[
    @method-name: get_status
    @method-desc: Get status of fan.

    @out number   speed             风扇转速.
    @out bool     status            风扇状态，true:开启 false:关闭.

    @in-example:  {\"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":[\"\",\"fan\",\"get_status\",{}],\"id\":1}
    @out-example: {"id":1,"jsonrpc":"2.0","result":{"speed":2000,"status":true}}
--]]
M.get_status = function(params)
    local res = { speed = 0, status = false }

    if not fs.access("/proc/gl-hw-info/fan") then
        return res
    end

    local hwmon, cooling_device = utils.readfile("/proc/gl-hw-info/fan", "l"):match("(.+) (.+)")

    if fs.access("/sys/class/hwmon/" .. hwmon .. "/fan1_input") then
        res.speed = utils.readfile("/sys/class/hwmon/" .. hwmon .. "/fan1_input", "n")
    else
        utils.writefile("/sys/class/fan/fan_speed", "refresh")
        ngx.sleep(1.5)
        res.speed = tonumber(utils.readfile("/sys/class/fan/fan_speed"):match("(%d+)"))
    end

    res.status = utils.readfile("/sys/class/thermal/" .. cooling_device .. "/cur_state", 'n') > 0
    return res
end

--[[
    @method-name: set_config
    @method-desc: Set fan config.

    @in number   temperature             风扇起转温度.
    @in-example:  {\"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":[\"\",\"fan\",\"set_config\",{\"temperature\":75}],\"id\":1}
    @out-example: {\"jsonrpc\": \"2.0\", \"id\": 1, \"result\": {}}
--]]

M.set_config = function(params)

    local c = uci.cursor()
    local temperature = params.temperature
    if temperature then
        c:set("glfan", "globals", "temperature", temperature)
        c:commit("glfan")
        ngx.pipe.spawn({"/etc/init.d/gl_fan", "restart"}):wait()
    end

end

--[[
    @method-name: get_config
    @method-desc: Get config of fan.

    @out number   temperature             当前风扇起转温度.
    @out number   warn_temperature        警告预值温度（出厂起转温度）.

    @in-example:  {\"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":[\"\",\"fan\",\"get_config\",{}],\"id\":1}
    @out-example: {"id":1,"jsonrpc":"2.0","result":{"warn_temperature":75,"temperature":75}}
--]]
M.get_config = function(params)

    local c = uci.cursor()
    local res = {}
    res.temperature = tonumber(c:get("glfan", "globals", "temperature") or "75")
    res.warn_temperature = tonumber(c:get("glfan", "globals", "warn_temperature") or "75")
    return res
end

return M
