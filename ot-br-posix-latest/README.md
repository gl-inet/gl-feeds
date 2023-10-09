# OpenThread Border Router

Platform designs use Radio Co-Processor (RCP),

<img src="https://openthread.io/static/platforms/images/ot-arch-rcp-vert.png" width="240" align='left' />

Confirmed supported targets

- GL.iNET S200 built-in Silicon Labs EFR32MG21 module
  - `configs/config-21.02.2.yml`

- GL.iNET AR750 + NRF52840 USB Dongle
  - `configs/config-21.02.2.yml`


## Getting started to build your own OTBR with [gl-infra-builder](https://github.com/gl-inet/gl-infra-builder)

```
$ git clone https://github.com/gl-inet/gl-infra-builder.git
$ cd gl-infra-builder/
$ python3 setup.py -c configs/config-21.02.2.yml
$ cd openwrt-21.02/openwrt-21.02.2/
$ ./scripts/gen_config.py glinet_s200_clean
$ make -j9
```

