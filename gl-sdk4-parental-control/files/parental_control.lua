--[[
    @object-name: parental-control
    @object-desc: 家长控制
--]]

local M = {}

local uci = require "uci"
local utils = require "oui.utils"

local function apply()
    os.execute("/etc/init.d/parental_control restart")
end

local function file_exists(path)
    local file = io.open(path, "rb")
    if file then file:close() end
    return file ~= nil
end

local function random_uci_section(uci_cursor,uci_type)
    local n
    local section
    math.randomseed(os.time())
    for _=1,1000,1 do
        n = math.random(1000000,9999999)
        section = uci_type .. tostring(n)
        if not uci_cursor:get("parental_control", section) then
            return section
        end
    end
    return nil
end

local function check_domains_valid(domains)
    for i = 1, #domains do
        if #domains[i] > 253 then
            return false
        end
    end
    return true
end

local function remove_mac_from_group(c,mac)
    c:foreach("parental_control", "group", function(s)
        if s.macs and type(s.macs) == "table" and #s.macs ~= 0  then
            for i = #s.macs, 1, -1 do -- 必须从后往前遍历，否则删除时由于数组长度变化将导致错误
                if string.upper(s.macs[i]) == mac then
                    table.remove(s.macs,i)
                    if #s.macs ~= 0 then
                        c:set("parental_control", s[".name"], "macs", s.macs)
                    else
                        c:delete("parental_control", s[".name"], "macs")
                    end
                end
            end
        end
    end)
end

--[[
    @method-type: call
    @method-name: get_app_list
    @method-desc: 获取可识别的应用列表。

    @out array   apps  可识别的应用列表。
    @out number  apps.id   应用id，全局唯一,应用ID从1001开始，0-1000为特殊ID，保留使用，其中1-8是类型ID，如果设置该ID则表示该类型下所有的应用。
    @out string  apps.name   应用名字。
    @out number  apps.type   应用类型（1:社交类，2:游戏类，3:视频类，4:购物类，5:音乐类，6:招聘类，7:下载类，8:门户网站）。


    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","get_app_list"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {"apps":[{"id":8001,"name":"baidu","type":8,},{"id":1002,"name":"facebook","type":1,}]}}
--]]
M.get_app_list = function()
    local ret = {}
    local apps = {}

    for line in io.lines("/etc/parental_control/app_feature.cfg") do
        local fields = {}
        local app = {}
        for field in line:gmatch("[^%s:]+") do
            fields[#fields + 1] = field
        end
        if #fields > 0  then
            if string.sub(fields[1],1,1) ~= "#" then
                app["id"] = tonumber(fields[1])
                app["name"] = fields[2]
                app["type"] = math.floor(tonumber(fields[1])/1000)
                apps[#apps + 1] = app
            end
        end
    end
    ret["apps"] = apps
    return ret
end

--[[
    @method-type: call
    @method-name: add_group
    @method-desc: 添加设备组。

    @in string   name 分组名字
    @in string   default_rule 分组使用的默认规则集ID，规则集ID需对应rules参数中返回的规则集ID。
    @in array    macs 分组包含的设备MAC地址列表，为字符串类型。
    @in array   ?schedules 分组包含的日程列表，如果对应分组存在日程设置则传入该参数。
    @in number   ?schedules.week 日程在每周的第几天，允许范围为1-7，依次对应周一到周末。
    @in string   ?schedules.begin 日程的开始时间，格式为hh:mm，起始时间必须在结束时间之前。
    @in string   ?schedules.end 日程的结束时间，格式为hh:mm，结束时间必须在起始时间之后。
    @in string   ?schedules.rule 该日程需要使用的规则集ID，规则集ID需对应rules参数中传入的规则集ID。

    @out string   id 新增分组的ID
    @out number ?err_code     错误码(-1: 缺少必须参数, -2:传递了shedules但是缺少必须参数)
    @out string ?err_msg      错误信息

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","add_group",{"name":"group1","macs":["98:6B:46:F0:9B:A4","98:6B:46:F0:9B:A5"],"default_rule":"cfga067b","schedules":[{"week":1,"begin":"12:00","end":"13:00","rule":"cfga067c"},{"date":2,"begin":"17:00","end":"18:00","rule":"cfga067c"}]}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {"id":"cfga048"}}
--]]
M.add_group = function(params)
    if params.name == nil or params.default_rule == nil or params.macs == nil then
        return {
            err_code = -1,
            err_msg = "parameter missing"
        }
    end

    local c = uci.cursor()

    local sid = c:add("parental_control", "group")
    local nsid = random_uci_section(c,"group") or sid;
    c:rename("parental_control", sid, nsid)
    sid = nsid
    c:set("parental_control", sid, "name", params.name)
    c:set("parental_control", sid, "default_rule", params.default_rule)
    if type(params.macs) == "table" and #params.macs ~= 0  then
        for i = 1, #params.macs do
            params.macs[i] = string.upper(params.macs[i])
            remove_mac_from_group(c,params.macs[i])
        end
        c:set("parental_control", sid, "macs",params.macs)
    end
    if type(params.schedules) == "table" and #params.schedules ~= 0  then
        for i = 1, #params.schedules do
            if params.schedules[i].week == nil or params.schedules[i].begin == nil or params.schedules[i]["end"] == nil or params.schedules[i].rule == nil then
                return {
                    err_code = -2,
                    err_msg = "schedule parameter missing"
                }
            end
            local sche = c:add("parental_control", "schedule")
            local nsche = random_uci_section(c,"sche") or sche;
            c:rename("parental_control", sche, nsche)
            sche = nsche
            c:set("parental_control", sche, "group",sid)
            c:set("parental_control", sche, "week",params.schedules[i].week)
            c:set("parental_control", sche, "begin",params.schedules[i].begin)
            c:set("parental_control", sche, "end",params.schedules[i]["end"])
            c:set("parental_control", sche, "rule",params.schedules[i].rule)
        end
    end
    c:commit("parental_control")
    apply()

    return {id=sid}
end

--[[
    @method-type: call
    @method-name: remove_group
    @method-desc: 移除设备组。

    @in string   id 需要删除的分组ID，分组ID通过get_config获取。

    @out number ?err_code     错误码(-1: 缺少必须参数)
    @out string ?err_msg      错误信息

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","remove_group",{"id":"cfga01234b"}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
M.remove_group = function(params)
    if params.id == nil then
        return {
            err_code = -1,
            err_msg = "parameter missing"
        }
    end
    local c = uci.cursor()

    if params.id then
        c:delete("parental_control", params.id)
    end
    c:foreach("parental_control", "schedule", function(s)
        if s.group and s.group ==params.id then
            c:delete("parental_control", s[".name"])
        end
    end)
    c:commit("parental_control")
    apply()

    return {}
end

--[[
    @method-type: call
    @method-name: set_group
    @method-desc: 修改设备组配置。

    @in string   id 需要设置的分组ID，分组ID通过get_config获取。
    @in string   name 分组名字
    @in string   default_rule 分组使用的默认规则集ID，规则集ID需对应rules参数中返回的规则集ID。
    @in array    macs 分组包含的设备MAC地址列表，为字符串类型。
    @in array   ?schedules 分组包含的日程列表，如果对应分组存在日程设置则传入该参数。
    @in array   ?schedules.week 日程在每周的第几天，允许范围为0-6，依次对应周末到周六。
    @in string   ?schedules.begin 日程的开始时间，格式为hh:mm，起始时间必须在结束时间之前。
    @in string   ?schedules.end 日程的结束时间，格式为hh:mm，结束时间必须在起始时间之后。
    @in string   ?schedules.rule 该日程需要使用的规则集ID，规则集ID需对应rules参数中传入的规则集ID。

    @out number ?err_code     错误码(-1: 缺少必须参数, -2:传递了shedules但是缺少必须参数)
    @out string ?err_msg      错误信息

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","set_group",{"id":"cfga01234b","name":"group1","macs":["98:6B:46:F0:9B:A4","98:6B:46:F0:9B:A5"],"default_rule":"cfga067b","schedules":[{"week":[1,3,5],"begin":"12:00","end":"13:00","rule":"cfga067c"},{"date":2,"begin":"17:00","end":"18:00","rule":"cfga067c"}]}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
M.set_group = function(params)
    if params.id == nil  then
        return {
            err_code = -1,
            err_msg = "parameter missing"
        }
    end
    local c = uci.cursor()

    local sid = params.id
    -- 如果传递了name参数则进行修改
    if params.name ~= nil then
        c:set("parental_control", sid, "name", params.name)
    end

    -- 如果传递了default_rule参数则进行修改
    if params.default_rule ~= nil then
        c:set("parental_control", sid, "default_rule", params.default_rule)
    end

    -- 如果传递了macs参数则进行修改
    if params.macs ~= nil then
      if type(params.macs) == "table" and #params.macs ~= 0  then
        for i = 1, #params.macs do
            params.macs[i] = string.upper(params.macs[i])
            remove_mac_from_group(c,params.macs[i])
        end
        c:set("parental_control", sid, "macs",params.macs)
      else
        c:delete("parental_control", sid, "macs")
      end
    end

    -- 如果传递了日程参数则进行修改
    if params.schedules ~= nil then
        -- 先删除旧的日程
        c:foreach("parental_control", "schedule", function(s)
          if s.group and s.group ==params.id then
              c:delete("parental_control", s[".name"])
          end
        end)
        -- 添加新日程
        if type(params.schedules) == "table" and #params.schedules ~= 0  then
          for i = 1, #params.schedules do
            if params.schedules[i].week == nil or params.schedules[i].begin == nil or params.schedules[i]["end"] == nil or params.schedules[i].rule == nil then
                return {
                    err_code = -2,
                    err_msg = "schedule parameter missing"
                }
            end
            local sche = c:add("parental_control", "schedule")
            local nsche = random_uci_section(c,"sche") or sche;
            c:rename("parental_control", sche,nsche)
            sche = nsche
            c:set("parental_control", sche, "group",sid)
            c:set("parental_control", sche, "week",params.schedules[i].week)
            if(#params.schedules[i].begin < 8) then
                c:set("parental_control", sche, "begin",params.schedules[i].begin .. ":00")
            else
                c:set("parental_control", sche, "begin",params.schedules[i].begin)
            end
            if(#params.schedules[i]["end"] < 8) then
                if #params.schedules[i]["end"] == "23:59" then
                    c:set("parental_control", sche, "end",params.schedules[i]["end"] .. ":59")
                else
                    c:set("parental_control", sche, "end",params.schedules[i]["end"] .. ":00")
                end
            else
                c:set("parental_control", sche, "end",params.schedules[i]["end"])
            end
            c:set("parental_control", sche, "rule",params.schedules[i].rule)
          end
        end
    end

    c:commit("parental_control")
    apply()

    return {}
end

--[[
    @method-type: call
    @method-name: add_rule
    @method-desc: 添加规则集。

    @in string  name   规则集的名字，全局唯一，用于区分不同的规则集。
    @in string  color   规则集的标签颜色，提供给显示UI使用。
    @in array   apps   规则集包含的应用的ID或应用类型，为整数类型，应用和ID的对应关系通过get_app_list接口返回。
    @in array   ?blacklist   规则集的黑名单列表，为字符串类型，该列表遵循应用特征描述语法，应用特征描述语法请参见doc.gl-inet.com

    @out number id     新建规则的ID
    @out number ?err_code     错误码(-1: 缺少必须参数, -2: blacklist中的域名不合法)
    @out string ?err_msg      错误信息

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","add_rule",{"name":"rule1","color":"#aabbccddee","apps":[1001,2002],"blacklist":["[tcp;;;www.google.com;;]"]}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
M.add_rule = function(params)
    if params.name == nil or  params.color == nil or  params.apps == nil then
        return {
            err_code = -1,
            err_msg = "parameter missing"
        }
    end
    local c = uci.cursor()

    local sid = c:add("parental_control", "rule")
    local nsid = random_uci_section(c,"rule") or sid;
    c:rename("parental_control", sid,nsid)
    sid = nsid
    c:set("parental_control", sid, "name", params.name)
    c:set("parental_control", sid, "color", params.color)
    if type(params.apps) == "table" and #params.apps ~= 0  then
        c:set("parental_control", sid, "apps",params.apps)
    end
    if type(params.blacklist) == "table" and #params.blacklist ~= 0  then
        if not check_domains_valid(params.blacklist) then
            return {
                err_code = -2,
                err_msg = "domain invalid"
            }
        end
        c:set("parental_control", sid, "blacklist",params.blacklist)
    end
    c:set("parental_control", sid, "action", "POLICY_DROP")
    c:commit("parental_control")
    apply()

    return {id=sid}
end

--[[
    @method-type: call
    @method-name: remove_rule
    @method-desc: 移除规则集。

    @in string   id 需要移除的规则ID，规则ID通过get_config获取。

    @out number ?err_code     错误码(-1: 缺少必须参数, -2: 被删除的规则正在被用作默认规则，-3: 被删除的规则正在被用作临时规则)
    @out string ?err_msg      错误信息

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","remove_rule",{"id":"cfga067b"}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
M.remove_rule = function(params)
    if params.id == nil then
        return {
            err_code = -1,
            err_msg = "parameter missing"
        }
    end
    local c = uci.cursor()

    local ret = {}
    ret["err_code"] = 0
    -- 检查待删除的规则是否被用于默认规则或临时规则
    c:foreach("parental_control", "group", function(s)
        if s.default_rule == params.id then
            ret["err_code"] = -2
        end
        if s.brief_rule == params.id then
            ret["err_code"] = -3
        end
    end)
    if ret["err_code"] ~= 0 then
        ret["err_msg"] = "rule are being used"
        return ret
    end

    -- 删除规则的关联日程
    c:foreach("parental_control", "schedule", function(s)
        if s.rule == params.id then
            c:delete("parental_control", s[".name"])
        end
    end)
    if params.id then
        c:delete("parental_control", params.id)
    end
    c:commit("parental_control")
    apply()

    return {}
end

--[[
    @method-type: call
    @method-name: set_rule
    @method-desc: 设置规则集。

    @in string   id 需要设置的规则ID，规则ID通过get_config获取。
    @in string  color   规则集的标签颜色，UI使用。
    @in string  name   规则集的名字，全局唯一，用于区分不同的规则集。
    @in array   apps   规则集包含的应用的ID或应用类型，为整数类型，应用和ID的对应关系通过get_app_list接口返回。
    @in array   ?blacklist   规则集的例外列表，为字符串类型，该列表相对于apps参数例外，一个规则集中最多添加32个例外特征, 遵循应用特征描述语法，应用特征描述语法请参见doc.gl-inet.com

    @out number ?err_code     错误码(-1: 缺少必须参数, -2: blacklist中的域名不合法)
    @out string ?err_msg      错误信息

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","set_rule",{"id":"cfga067b","name":"rule1","color":"#aabbccddee","apps":[1001,2002],"blacklist":["[tcp;;;www.google.com;;]"]}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
M.set_rule = function(params)
    if params.id == nil or params.name == nil or  params.color == nil or  params.apps == nil then
        return {
            err_code = -1,
            err_msg = "parameter missing"
        }
    end
    local c = uci.cursor()

    local sid = params.id
    c:set("parental_control", sid, "name", params.name)
    c:set("parental_control", sid, "color", params.color)
    if type(params.apps) == "table" and #params.apps ~= 0  then
        c:set("parental_control", sid, "apps",params.apps)
    end
    if type(params.blacklist) == "table" and #params.blacklist ~= 0  then
        if not check_domains_valid(params.blacklist) then
            return {
                err_code = -2,
                err_msg = "domain invalid"
            }
        end
        c:set("parental_control", sid, "blacklist",params.blacklist)
    else
        c:delete("parental_control", sid, "blacklist")
    end
    c:commit("parental_control")
    apply()

    return {}
end

local function key_in_array(list,key)
    if list then
        for k, _ in pairs(list) do
          if k == key then
           return true
          end
        end
    end
end

--[[
    @method-type: call
    @method-name: get_config
    @method-desc: 获取家长控制参数配置。

    @out bool     enable  是否使能。
    @out bool     drop_anonymous  是否禁止匿名设备访问。
    @out bool     auto_update  是否自动更新特征库。
    @out bool     enable_app  是否使能app特征库。
    @out array   ?rules  规则集列表,如果规则集不为空则返回。
    @out string  ?rules.id   规则集ID，全局唯一，用于区分不同的规则集。
    @out bool    ?rules.preset   是否为预置规则。
    @out string  ?rules.name   规则集的名字。
    @out string  ?rules.color   规则集的标签颜色，UI使用。
    @out array   ?rules.apps   规则集包含的应用的ID列表，为整数类型，应用和ID的对应关系通过get_app_list接口返回。
    @out array   ?rules.blacklist   规则集的例外列表，为字符串类型，该列表相对于apps参数例外，遵循应用特征描述语法，应用特征描述语法请参见doc.gl-inet.com
    @out array   ?groups 设备分组列表,如果分组列表不为空则返回。
    @out string   ?groups.id 分组ID，全局唯一，用于区分不同的设备组。
    @out string   ?groups.name 分组名字。
    @out string   ?groups.default_rule 分组使用的默认规则集ID，规则集ID需对应rules参数中返回的规则集ID。
    @out array   ?groups.macs 分组包含的设备MAC地址列表，为字符串类型。
    @out array   ?groups.schedules 分组包含的日程列表，如果对应分组存在日程设置则返回该参数。
    @out number   ?groups.schedules.id 日程ID。
    @out number   ?groups.schedules.week 日常在每周的第几天，允许范围为0-6，依次对应周末到周六。
    @out string   ?groups.schedules.begin 日程的开始时间，格式为hh:mm，起始时间必须在结束时间之前。
    @out string   ?groups.schedules.end 日程的结束时间，格式为hh:mm，结束时间必须在起始时间之后。
    @out string   ?groups.schedules.rule 该日程需要使用的规则集ID，规则集ID需对应rules参数中返回的规则集ID。


    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","get_config"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {"enable":true,"drop_anonymous":false,"auto_update":false,"rules":[{"id":"cfga067b","name":"rule1","color":"#aabbccddee","apps":[1001,2002],"blacklist":["[tcp;;;www.google.com;;]"]},{"id":"cfga067c","name":"rule2","color":"#aabbccddee","apps":[3003,4004],"blacklist":["[tcp;;;www.google.com;;]"]}],"groups":[{"name":"group1","macs":["98:6B:46:F0:9B:A4","98:6B:46:F0:9B:A5"],"default_rule":"cfga067a","schedules":[{"week":1,"begin":"12:00","end":"13:00","rule":"cfga067c","id":"cfga066c"},{"date":2,"begin":"14:00","end":"15:00","rule":"cfga067c","id":"cfga076c"}]}]}}
--]]
M.get_config = function()
    local c = uci.cursor()
    local ret = {}
    local enable = c:get("parental_control", "global", "enable") or '0'
    local drop_anonymous = c:get("parental_control", "global", "drop_anonymous") or '0'
    local auto_update = c:get("parental_control", "global", "auto_update") or '0'
    local enable_app = c:get("parental_control", "global", "enable_app") or '0'
    local rules ={}
    local groups ={}
    c:foreach("parental_control", "rule", function(s)
        local rule = {}
        rule["id"] = s[".name"]
        rule["preset"] = s.preset == "1"
        rule["name"] = s.name
        rule["color"] = s.color or "#FFFFFFFF"
        if s.apps ~= nil then
            for i=1,#s.apps do
                s.apps[i]=tonumber(s.apps[i])
            end
        end
        rule["apps"] = s.apps
        if s.blacklist then
            rule["blacklist"] = s.blacklist
        end
        rules[#rules + 1] = rule
    end)
    c:foreach("parental_control", "group", function(s)
        local group = {}
        group["id"] = s[".name"]
        group["name"] = s.name
        group["default_rule"] = s.default_rule
        if s.macs then
            group["macs"] = s.macs
        end
        groups[#groups + 1] = group
    end)

    c:foreach("parental_control", "schedule", function(s)
        local schedule = {}
        schedule["id"] = s[".name"]
        schedule["group"] = s.group
        schedule["week"] = tonumber(s.week)
        schedule["rule"] = s.rule
        schedule["begin"] = s.begin
        schedule["end"] = s["end"]
        if #groups then
            for i=1,#groups do
                if groups[i]["id"] == s.group then
                    if not key_in_array(groups[i],"schedules") then
                        groups[i]["schedules"] ={}
                    end
                    groups[i]["schedules"][#groups[i]["schedules"]+1] = schedule
                    break
                end
            end
        end
    end)

    ret["enable"] = enable ~= "0"
    ret["drop_anonymous"] = drop_anonymous ~= "0"
    ret["auto_update"] = auto_update ~= "0"
    ret["enable_app"] = enable_app ~= "0"
    ret["rules"] = rules
    ret["groups"] = groups

    return ret
end


--[[
    @method-type: call
    @method-name: set_config
    @method-desc: 设置基础配置。
    @in bool     enable  是否使能。
    @in bool     drop_anonymous  是否禁止匿名设备访问。
    @in bool     auto_update  是否自动更新特征库。

    @out number ?err_code     错误码(-1: 缺少必须参数)
    @out string ?err_msg      错误信息

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","set_config",{"enable":true,"drop_anonymous":false,"auto_update":false}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
M.set_config = function(params)
    if params.enable == nil or params.drop_anonymous == nil or  params.auto_update == nil then
        return {
            err_code = -1,
            err_msg = "parameter missing"
        }
    end
    local c = uci.cursor()

    c:set("parental_control", "global", "enable", params.enable and "1" or "0")
    c:set("parental_control", "global", "drop_anonymous", params.drop_anonymous and "1" or "0")
    c:set("parental_control", "global", "auto_update", params.auto_update and "1" or "0")
    c:commit("parental_control")
    apply()

    return {}
end
--[[
    @method-type: call
    @method-name: set_brief
    @method-desc: 设置临时规则。
    @in bool     enable  是否使能。
    @in bool     ?manual_stop  需要手动停止。
    @in string   ?time  临时规则持续的时间。
    @in string   ?rule_id  临时规则的规则ID。
    @in string   group_id    需要设置的分组ID，分组ID通过get_config获取。

    @out number ?err_code     错误码(-1: 缺少必须参数)
    @out string ?err_msg      错误信息

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","set_brief",{"enable":true,"manual_stop":false,"time":"18:00","rule_id":"cfg0036","group_id":"cfg2560"}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
M.set_brief = function(params)
    if params.group_id == nil or params.enable == nil then
        return {
            err_code = -1,
            err_msg = "parameter missing"
        }
    end
    local c = uci.cursor()
    local sid = params.group_id

    if params.enable then
        if params.manual_stop then
            c:set("parental_control", sid, "brief_time", "0")
        else
            if(#params.time < 8) then
                c:set("parental_control", sid, "brief_time", params.time  .. ":00")
            else
                c:set("parental_control", sid, "brief_time", params.time)
            end
        end
        c:set("parental_control", sid, "brief_rule", params.rule_id)
    else
        c:delete("parental_control", sid, "brief_time")
        c:delete("parental_control", sid, "brief_rule")
    end
    c:commit("parental_control")
    apply()

    return {}
end

--[[
    @method-type: call
    @method-name: get_brief
    @method-desc: 获取临时规则配置。
    @in string   group_id    需要获取的分组ID，分组ID通过get_config获取。

    @out bool     enable  是否使能。
    @out bool     ?manual_stop  需要手动停止。
    @out string   ?time  临时规则持续的时间。
    @out string   ?rule_id  临时规则的规则ID。
    @out number ?err_code     错误码(-1: 缺少必须参数)
    @out string ?err_msg      错误信息

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","get_brief"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {"enable":true,"manual_stop":false,"time":"18:00","rule_id":"cfg0036","group_id":"cfg2560"}}
--]]
M.get_brief = function(params)
    if params.group_id == nil then
        return {
            err_code = -1,
            err_msg = "parameter missing"
        }
    end
    local ret = {}
    local c = uci.cursor()
    local sid = params.group_id
    local time = c:get("parental_control", sid, "brief_time")

    if time ~= nil then
        ret["enable"] = true
        ret["rule_id"] = c:get("parental_control", sid, "brief_rule")
        if time == "0" then
            ret["manual_stop"] = true
        else
            ret["manual_stop"] = false
            ret["time"] = time
        end
    else
        ret["enable"] = false
    end

    return ret
end

--[[
    @method-type: call
    @method-name: get_status
    @method-desc: 设置临时规则。

    @out bool  time_valid 系统时间是否已经同步（0：时间未同步，1：时间已同步）
    @out array   groups    用户组列表。
    @out string   groups.id    分组ID。
    @out string   groups.rule  当前正在使用的规则ID。
    @out bool   ?groups.brief  是否正在使用临时规则。
    @out string   ?schedule_id  正在使用的日程ID，如果没有返回则表示当前使用的规则为默认规则或临时规则。
    @out number ?err_code     错误码(-1: 缺少必须参数)
    @out string ?err_msg      错误信息

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","get_status"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {"time_valid":true,groups:[{"id":"cfg1356","rule":"drop","brief":false}]}
--]]
M.get_status = function()
    local ret = {}
    local groups = {}
    local c = uci.cursor()

    if file_exists("/var/state/dnsmasqsec") then
        ret["time_valid"] = true
    else
        ret["time_valid"] = false
    end

    if file_exists("/proc/parental-control/group") then
      for line in io.lines("/proc/parental-control/group") do
        local fields = {}
        local group = {}
        for field in line:gmatch("[^\t]+") do
            fields[#fields + 1] = field
        end
        if #fields > 2 and fields[3] ~= "MACs"  then
            group["id"] = fields[1]
            group["rule"] = fields[2]
            group["brief"] = c:get("parental_control", fields[1], "brief_time") ~= nil
            groups[#groups + 1] = group
            if file_exists("/var/run/pc_schedule/" .. fields[1]) then
                local sch_id = utils.readfile("/var/run/pc_schedule/" .. fields[1]);
                if sch_id then
                    group["schedule_id"] = string.gsub(sch_id, "\n", "")
                end
            end
        end
      end
    end
    ret["groups"] = groups
    return ret
end

--[[
    @method-type: call
    @method-name: update
    @method-desc: 手动更新特征库。


    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","update"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
M.update = function()
    return {}
end

return M
