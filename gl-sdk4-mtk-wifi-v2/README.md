# gl-sdk4-mtk-wifi-v2

```
root@GL-MT3000:~# ubus call network.wireless status
{
        "mt798111": {
                "up": true,
                "pending": false,
                "autostart": true,
                "disabled": false,
                "retry_setup_failed": false,
                "config": {
                        "band": "2g",
                        "htmode": "HE40",
                        "channel": "auto",
                        "txpower": 100,
                        "country": "US",
                        "disabled": false
                },
                "interfaces": [
                        {
                                "section": "wifi2g",
                                "config": {
                                        "mode": "ap",
                                        "ifname": "ra0",
                                        "ssid": "GL-MT3000-241",
                                        "encryption": "psk2",
                                        "key": "goodlife",
                                        "isolate": false,
                                        "mode": "ap",
                                        "network": [
                                                "lan"
                                        ],
                                        "isolate": false,
                                        "disabled": false
                                },
                                "vlans": [

                                ],
                                "stations": [

                                ]
                        }
                ]
        },
        "mt798112": {
                "up": true,
                "pending": false,
                "autostart": true,
                "disabled": false,
                "retry_setup_failed": false,
                "config": {
                        "band": "5g",
                        "txpower": 100,
                        "country": "US",
                        "hwmode": "11a",
                        "htmode": "HE80",
                        "channel": "60",
                        "disabled": false
                },
                "interfaces": [
                        {
                                "section": "wifi5g",
                                "config": {
                                        "mode": "ap",
                                        "ifname": "rax0",
                                        "ssid": "GL-MT3000-241-5G",
                                        "encryption": "psk2",
                                        "key": "goodlife",
                                        "isolate": false,
                                        "hidden": false,
                                        "mode": "ap",
                                        "network": [
                                                "lan"
                                        ],
                                        "isolate": false,
                                        "disabled": false
                                },
                                "vlans": [

                                ],
                                "stations": [

                                ]
                        },
                        {
                                "section": "guest5g",
                                "config": {
                                        "mode": "ap",
                                        "ifname": "rax1",
                                        "encryption": "psk2",
                                        "key": "goodlife",
                                        "ssid": "GL-MT3000-241-5G-Guest",
                                        "isolate": true,
                                        "network": [
                                                "guest"
                                        ],
                                        "mode": "ap",
                                        "isolate": true,
                                        "disabled": false
                                },
                                "vlans": [

                                ],
                                "stations": [

                                ]
                        }
                ]
        }
}
```
