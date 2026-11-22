include $(TOPDIR)/rules.mk

PKG_NAME:=inih
PKG_VERSION:=62
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/benhoyt/inih.git
PKG_SOURCE_VERSION:=r$(PKG_VERSION)
PKG_MIRROR_HASH:=aef911f559895464c9da12b68344a58141af1997eb2c9cc9df4e9b25853e7dab

PKG_MAINTAINER:=Tianling Shen <cnsztl@immortalwrt.org>
PKG_LICENSE:=BSD-3-Clause
PKG_LICENSE_FILES:=LICENSE.txt

include $(INCLUDE_DIR)/package.mk

define Package/libinih/Default
  SECTION:=libs
  CATEGORY:=Libraries
  URL:=https://github.com/benhoyt/inih
endef

define Package/libinih
  $(call Package/libinih/Default)
  TITLE:=Simple .INI file parser in C
endef

define Package/libinireader
  $(call Package/libinih/Default)
  TITLE:=C++ library and API for inih
  DEPENDS:=+libinih +libstdcpp
endef

define Package/libinih/description
  inih (INI Not Invented Here) is a simple .INI file parser written
  in C. It's only a couple of pages of code, and it was designed to
  be small and simple, so it's good for embedded systems. It's also
  more or less compatible with Python's ConfigParser style of .INI
  files, including RFC 822-style multi-line syntax and name: value
  entries.
endef

Package/libinireader/description = $(Package/libinih/description)

define Build/Configure
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CPPFLAGS) $(TARGET_CFLAGS) -fPIC -fvisibility=hidden \
		-c $(PKG_BUILD_DIR)/ini.c -o $(PKG_BUILD_DIR)/ini.o
	$(TARGET_CXX) $(TARGET_CPPFLAGS) $(TARGET_CXXFLAGS) -std=c++11 -fPIC -fvisibility=hidden \
		-c $(PKG_BUILD_DIR)/cpp/INIReader.cpp -o $(PKG_BUILD_DIR)/INIReader.o

	$(TARGET_AR) rcs $(PKG_BUILD_DIR)/libinih.a $(PKG_BUILD_DIR)/ini.o
	$(TARGET_AR) rcs $(PKG_BUILD_DIR)/libINIReader.a $(PKG_BUILD_DIR)/INIReader.o
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -shared -Wl,-soname,libinih.so.0 \
		-o $(PKG_BUILD_DIR)/libinih.so.0 $(PKG_BUILD_DIR)/ini.o
	$(LN) libinih.so.0 $(PKG_BUILD_DIR)/libinih.so
	$(TARGET_CXX) $(TARGET_CXXFLAGS) $(TARGET_LDFLAGS) -shared -Wl,-soname,libINIReader.so.0 \
		-o $(PKG_BUILD_DIR)/libINIReader.so.0 $(PKG_BUILD_DIR)/INIReader.o \
		-L$(PKG_BUILD_DIR) -linih
	$(LN) libINIReader.so.0 $(PKG_BUILD_DIR)/libINIReader.so

	$(INSTALL_DIR) $(PKG_INSTALL_DIR)/usr/include $(PKG_INSTALL_DIR)/usr/lib/pkgconfig
	$(CP) $(PKG_BUILD_DIR)/ini.h $(PKG_BUILD_DIR)/cpp/INIReader.h $(PKG_INSTALL_DIR)/usr/include/
	$(CP) $(PKG_BUILD_DIR)/lib{inih,INIReader}.{a,so*} $(PKG_INSTALL_DIR)/usr/lib/
	printf '%s\n' \
		'Name: inih' \
		'Description: simple .INI file parser' \
		'Version: $(PKG_VERSION)' \
		'Libs: -L/usr/lib -linih' \
		'Cflags: -I/usr/include' \
		> $(PKG_INSTALL_DIR)/usr/lib/pkgconfig/inih.pc
	printf '%s\n' \
		'Name: INIReader' \
		'Description: simple .INI file parser for C++' \
		'Version: $(PKG_VERSION)' \
		'Requires.private: inih' \
		'Libs: -L/usr/lib -lINIReader' \
		'Cflags: -I/usr/include' \
		> $(PKG_INSTALL_DIR)/usr/lib/pkgconfig/INIReader.pc
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include
	$(CP) $(PKG_INSTALL_DIR)/usr/include/{ini,INIReader}.h $(1)/usr/include
	$(INSTALL_DIR) $(1)/usr/lib/pkgconfig
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/pkgconfig/{inih,INIReader}.pc $(1)/usr/lib/pkgconfig
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/lib{inih,INIReader}.a $(1)/usr/lib/
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/lib{inih,INIReader}.so* $(1)/usr/lib/
endef

define Package/libinih/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libinih.so* $(1)/usr/lib/
endef

define Package/libinireader/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libINIReader.so* $(1)/usr/lib/
endef

$(eval $(call BuildPackage,libinih))
$(eval $(call BuildPackage,libinireader))
