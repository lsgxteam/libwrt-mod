#
# Copyright (C) 2014 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=fan-monitor
PKG_VERSION:=2
PKG_RELEASE:=6
PKG_LICENSE:=GPLv3
PKG_LICENSE_FILES:=LICENSE

include $(INCLUDE_DIR)/package.mk

define Package/fan-monitor
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Fan Monitor Control
  PKGARCH:=all
endef

define Package/fan-monitor/description
  Fan Monitor Control.
endef

define Build/Compile
endef

define Package/fan-monitor/install
	$(INSTALL_DIR) $(1)/etc/init.d $(1)/usr/sbin
	$(INSTALL_BIN) ./files/fan-monitor.init $(1)/etc/init.d/fan-monitor
	$(INSTALL_BIN) ./files/fan-monitor.sh $(1)/usr/sbin/fan-monitor
endef

$(eval $(call BuildPackage,fan-monitor))
