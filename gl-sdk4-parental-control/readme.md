# parental-control
## What this project does
This project implements the parental control function. There are many useful open source projects to implement parental control, but some of them are too simple to realize the application of DPI, and some projects are too heavy and need to occupy a lot of resources. Therefore, I developed this project, which can realize simple and efficient DPI function without occupying a lot of hardware resources. This project works in kernel space, and almost all devices running openwrt system can work.

This project is currently integrated into the glinet 4.2.0 firmware or higher.

![play](./img/play.gif)

## How to compile
We need to clone this project into openwrt's package directory and then, from menuconfig, select **kmod-gl-sdk4-parental control**.
Because it is a kernel module, it usually needs to be compiled with firmware.

If you are sure that your current firmware already contains the ipt-conntrack related symbols, you can also install the compiled kmod directly by using the following method.

First, get the hash of the current kernel in the firmware with the following command

```
opkg info  kernel
```

Returns the result might be as follows, the **57d388dbd346719758aae2131362f842** is the kernel hash

```
root@GL-AXT1800:~# opkg info  kernel
Package: kernel
Version: 4.4.60-1-57d388dbd346719758aae2131362f842
Depends: libc
Status: install user installed
Architecture: arm_cortex-a7
Installed-Time: 1672139411
```

We can then compile using this hash value

```
make package/parental-control/compile LINUX_VERMAGIC:=57d388dbd346719758aae2131362f842
```



## How to use
### use the shcedule
Under the openwrt system, all configurations are managed through the uci.
We already preset the accept and drop rules, we can also create our own using the following command.

```
uci set parental_control.myrule=rule
uci set parental_control.myrule.name='myrule'
uci add_list parental_control.myrule.blacklist='google.com'
uci set parental_control.myrule.action='POLICY_DROP'
```
Next, create a device group to manage your devices.
```
uci set parental_control.mygroup=group
uci set parental_control.mygroup.name='mygroup'
uci set parental_control.mygroup.default_rule='myrule'
uci add_list parental_control.mygroup.macs='72:B7:xx:xx:xx:xx'
```
Next, we can create a schedule to dynamically switch the rule.
```
uci set parental_control.mysche='schedule'
uci set parental_control.mysche.week='6'
uci set parental_control.mysche.begin='00:00:00'
uci set parental_control.mysche.end='23:59:00'
uci set parental_control.mysche.rule='accept'
```
Finally, please make sure your time zone is synchronized and we'll enable it
```
uci set parental_control.global.enable='1'
uci commit
/etc/init.d/parental_control restart
```
### check working status
When you see the **/proc/parents-control/** folder created, it means the program is working.

**/proc/parental-control/rule** will show all rule states.

**/proc/parental-control/group** will show all group states.

**/proc/parental-control/app**  will show all app states.

**/proc/parental-control/drop_anonymous** will show the Internet status of the anonymous device.

**/proc/parental-control/src_dev** will show  the network interface to be matched.

### use the app feature library
**/proc/parental-control/app** show the currently loaded app feature library, which we can use in rule by id, for example

```
uci add_list parental_control.myrule.apps='1001'
uci add_list parental_control.myrule.apps='1002'
uci add_list parental_control.myrule.apps='1003'
uci commit
/etc/init.d/parental_control restart
```
### use the brief rule
The brief rule is used to temporarily switch rule. This rule can last for a specified period of time and then will be cleared automatically. If you want to keep using the brief rule,  set the brief_time parameter to 0.
```
uci set parental_control.mygroup.brief_time='02:52:00'
uci set parental_control.mygroup.brief_rule='myrule'
uci commit
/etc/init.d/parental_control restart
```
### Configuration  description

Configure applications in the /etc/config/parental_contorl file. The configuration file is described as follows.

#### global

| Name           | Required | Description                                                  |
| -------------- | -------- | ------------------------------------------------------------ |
| enable         | Y        | Boolean; Whether to enable the application                   |
| drop_anonymous | Y        | Boolean; Whether to deny anonymous devices access to the Internet |
| auto_update    | Y        | Boolean; Whether to automatically update the APP feature library |
| src_dev        | N        | List; By default, the packets sent from all network interfaces are matched. If **src_dev** is specified, only the packets sent from a specific network interface are matched |
| update_time    | N        | String; Update time of APP feature library                   |
| update_url     | N        | String; Get the update URL of APP feature library            |
| enable_app     | N        | Boolean; Use it for glinet UI                                |



### rule

| Name      | Required | Description                                                  |
| --------- | -------- | ------------------------------------------------------------ |
| name      | N        | String; The name of the rule, no use                         |
| action    | Y        | String; Action used to set the rule. Possible values are DROP,ACCEPT,POLICY_DROP,POLICY_ACCEPT. Other values are ignored. |
| apps      | N        | List; List of application ids to be matched by the rule      |
| blacklist | N        | List; A blacklist list of rules that will be matched in preference to apps. Each element can be a URL or a APP feature library syntax. |
| color     | N        | String; Use it for glinet UI                                 |
| preset    | N        | Boolean; Use it for glinet UI                                |



### group

| Name         | Required | Description                                                  |
| ------------ | -------- | ------------------------------------------------------------ |
| name         | N        | String; The name of the group, no use                        |
| default_rule | Y        | String; Must correspond to the uci section field of a rule. Note that it is **not name** of rule |
| macs         | N        | List; MAC address list of the  group, the format is xx:xx:xx:xx:xx:xx |
| brief_rule   | N        | String; Temporary rules will be automatically deleted when the brief_time condition is met. Must correspond to the uci section field of a rule. |
| brief_time   | N        | String; The duration of the brief_rule. If this value is 0, it will never end |



### schedule

| Name  | Required | Description                                                  |
| ----- | -------- | ------------------------------------------------------------ |
| group | Y        | String; For which group it is valid, it must correspond to the uci section field of a group. Note that it is **not name** of group |
| week  | Y        | String; On which day of the week it takes effect. The allowable range is 0 to 6, which corresponds to Sunday to Saturday. The format can be '3' or '3 4 5' |
| begin | Y        | String; The start time of the schedule. format is hh:mm:ss   |
| end   | Y        | String; The end time of the schedule, must be later than the begin time. format is hh:mm:ss; |
| rule  | Y        | The rule to be used in the begin-end period must correspond to the uci section field of a rule. |


## How is working



![framework](./img/framework.svg)

The application layer mainly provides configuration, external API and implements policy switching at different times.

The kernel adds a hook at the forward of linux netfilter to realize a filter. By maintaining the binding relationship between group,rule, app and other 3 linked list elements, the kernel realizes the requirement of device group,rule set and application filtering.

The proc file node mainly provides states for easy debugging and state retrieval.

The interaction between the application layer and the kernel layer is implemented through the **/dev/parental_control** device

### Reference relationship

The reference relationships of group,rule, and app in /dev/parental_control are as follows.

![relationship ](./img/relationship.svg)

### Filter flow

Filters are designed to ensure that all data passes through efficiently and minimize the consumption of data analysis on network performance.

![filter-flow](./img/filter-flow.svg)



## APP feature library syntax

The  feature library is in txt text format. Each line represents an application. A complete application description includes the ID, application name, and feature set.

A feature set can consist of a single feature or multiple features separated by commas.

#### APP description syntax

```
id name:[feature1,feature2，feature3]
```

| Name    | Description                                                  |
| ------- | ------------------------------------------------------------ |
| id      | Application ID, which is globally unique, is a field used to distinguish different applications. When adding an app, the maximum ID of the group needs to be increased by 1. For example, the current maximum value of the appid of the chat class is 1005. <br />appid Components: class id and app id<br /> class id=appid/1000 Integer, for example, the class id of 8001 is 8 |
| name    | The name of the application is for visual reading only. It is not used to distinguish between applications. Such as wechat, Baidu and so on |
| feature | The packet feature description can contain content such as protocol, port, url, and data dictionary. For details, see the **feature  syntax**. |



#### feature syntax

``` 
proto;sport;dport;host;request;dict1|dict2|dict3
```

| Name    | Description                                                  |
| ------- | ------------------------------------------------------------ |
| proto   | Transport layer protocol (tcp or udp)                        |
| sport   | The source port                                              |
| dport   | The destination port can be a single port, for example, 8888, or a port range, for example, 8888-9999. The destination port can be reversed by an exclamation mark, for example,! 8888 or! 8888-9999. |
| host    | Domain names support fuzzy matching. If you want to filter www.baidu.com, only baidu can be entered. |
| request | Request resources keyword <br / > such as request www.baidu.com/images/test.png <br / > you can configure the set request to images/test.png<br / > Note that only supports HTTP request field, https not supported |
| dict    | Data dictionary description, can pass packets in different position of data values to match the application, a feature can include multiple data dictionary, use '\|' segmentation between multiple data dictionary. For details, see the **dict  syntax**. |



#### dict syntax

```
position:value
```

| Name     | Description                                                  |
| -------- | ------------------------------------------------------------ |
| position | The relative position (in decimal) of the data field in a network packet. If the field is negative, it indicates the position in front of the data. For example, -1 indicates the offset one byte from the beginning of the data field in the packet, and if the field is positive, it indicates the offset back. |
| value    | Data value corresponding to position (in hexadecimal)        |

#### A complete example
```
4005 mogu:[tcp;;;mogujie;;,tcp;;;mogucdn;;,tcp;;;;;00:73|01:ea|02:68|03:fb|04:3f] 
```
In the above example, the appid is 4005 and the app name is mogu.
The app has three features. The first two features match the domain name, and the later features will compare the data of the specified position in the data packet.


## Submit app feature
I will save all the app features in the open source project. If you want to add your app to the database, you can submit your app feature through a pull request.


