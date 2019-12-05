#
# Copyright (C) 2013-2016 OpenWrt.org
# Copyright (C) 2016 Yousong Zhou
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
ifeq ($(SUBTARGET),cortexa8)

define Device/olimex_a10-olinuxino-lime
  DEVICE_VENDOR := Olimex
  DEVICE_MODEL := A10-OLinuXino-LIME
  DEVICE_PACKAGES:=kmod-ata-core kmod-ata-sunxi kmod-sun4i-emac kmod-rtc-sunxi
  SUNXI_DTS:=sun4i-a10-olinuxino-lime
endef

TARGET_DEVICES += olimex_a10-olinuxino-lime


define Device/olimex_a13-olimex-som
  DEVICE_VENDOR := Olimex
  DEVICE_MODEL := A13-SOM
  DEVICE_PACKAGES:=kmod-rtl8192cu
  SUPPORTED_DEVICES:=olimex,a13-olinuxino
  SUNXI_DTS:=sun5i-a13-olinuxino
endef

TARGET_DEVICES += olimex_a13-olimex-som


define Device/olimex_a13-olinuxino
  DEVICE_VENDOR := Olimex
  DEVICE_MODEL := A13-OLinuXino
  DEVICE_PACKAGES:=kmod-rtl8192cu
  SUNXI_DTS:=sun5i-a13-olinuxino
endef

TARGET_DEVICES += olimex_a13-olinuxino


define Device/cubietech_a10-cubieboard
  DEVICE_VENDOR := Cubietech
  DEVICE_MODEL := Cubieboard
  DEVICE_PACKAGES:=kmod-ata-core kmod-ata-sunxi kmod-sun4i-emac kmod-rtc-sunxi
  SUNXI_DTS:=sun4i-a10-cubieboard
endef

TARGET_DEVICES += cubietech_a10-cubieboard


define Device/linksprite_a10-pcduino
  DEVICE_VENDOR := LinkSprite
  DEVICE_MODEL := pcDuino
  DEVICE_PACKAGES:=kmod-sun4i-emac kmod-rtc-sunxi kmod-rtl8192cu
  SUNXI_DTS:=sun4i-a10-pcduino
endef

TARGET_DEVICES += linksprite_a10-pcduino


define Device/marsboard_a10-marsboard
  DEVICE_VENDOR := HAOYU Electronics
  DEVICE_MODEL := MarsBoard A10
  DEVICE_PACKAGES:=mod-ata-core kmod-ata-sunxi kmod-sun4i-emac kmod-rtc-sunxi sound-soc-sunxi
  SUNXI_DTS:=sun4i-a10-marsboard
endef

TARGET_DEVICES += marsboard_a10-marsboard

endif
