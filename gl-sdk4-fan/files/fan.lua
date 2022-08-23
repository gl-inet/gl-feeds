--[[
    @object-name: fan
    @object-desc: This is the API related to fan Internet access.
--]]

local uci = require "uci"
local utils = require "oui.utils"

local M = {}
 
local function stop_test_fan()
    utils.writefile("/sys/class/thermal/cooling_device0/cur_state", "0")
    ngx.pipe.spawn({"/etc/init.d/gl_fan", "restart"}):wait()
end

--[[
    @method-name: set_test
    @method-desc: Set fan test.

    @in bool     test_fan              测试风扇起转.
    @in number   test_time             测试风扇起转时间s, 缺省为10s.
    @in-example:  {\"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":[\"\",\"fan\",\"set_test\",{\"test_fan\":true,\"test_time\":5}],\"id\":1}
    @out-example: {\"jsonrpc\": \"2.0\", \"id\": 1, \"result\": {}}
--]]

M.set_test = function(params)

    local test_fan = params.test_fan
    if test_fan == true then
        local test_time = params.test_time or 10
        ngx.pipe.spawn({"/etc/init.d/gl_fan", "stop"}):wait()
        utils.writefile("/sys/class/thermal/cooling_device0/cur_state", "255")
        ngx.timer.at(test_time, stop_test_fan)
    end
    
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

    @in  bool     get_speed             是否获取风扇转速，true:是 false 否.
    @out number   fan_speed             风扇转速.
    @out bool     fan_config            风扇状态，true:开启 false:关闭.
    @out number   temperature           风扇起转温度.

    @in-example:  {\"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":[\"\",\"fan\",\"get_config\",{\"get_speed\":true}],\"id\":1}
    @out-example: {"id":1,"jsonrpc":"2.0","result":{"fan_speed":2000,"fan_status":true,"temperature":75}}
--]]
M.get_config = function(params)

    local c = uci.cursor()
    local res = {}
    local get_speed = params.get_speed
    if get_speed == true then
        utils.writefile("/sys/class/fan/fan_speed", "refresh")
        ngx.sleep(1.5)
        res.fan_speed = tonumber(utils.readfile("/sys/class/fan/fan_speed"):match("(%d+)"))
    end

    res.status = tonumber(utils.readfile("/sys/class/thermal/cooling_device0/cur_state", 'n') or 0) ~= 0 and true or false
    res.temperature = tonumber(c:get("glfan", "globals", "temperature") or "0")
    return res
end

return M
