#
# Copyright (C) 2006-2016 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mgmtlanip
PKG_VERSION:=1.0.5
PKG_RELEASE:=20220315
PKG_LICENSE:=GPLv3
PKG_LICENSE_FILES:=LICENSE

include $(INCLUDE_DIR)/package.mk

define Package/mgmtlanip
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Mgmt LanIP Control
  PKGARCH:=all
endef

define Package/mgmtlanip/description
  Mgmt LanIP Control.
endef

define Package/mgmtlanip/conffiles
/etc/config/mgmtlanip
endef

define Build/Compile
endef

define Package/mgmtlanip/install
	$(INSTALL_DIR) $(1)/etc/init.d $(1)/etc/config $(1)/usr/bin
	$(INSTALL_CONF) ./files/mgmtlanip.conf $(1)/etc/config/mgmtlanip
	$(INSTALL_BIN) ./files/mgmtlanip.init $(1)/etc/init.d/mgmtlanip
	$(INSTALL_BIN) ./files/mgmtlanip.sh $(1)/usr/bin/mgmtlanip
endef

$(eval $(call BuildPackage,mgmtlanip))
