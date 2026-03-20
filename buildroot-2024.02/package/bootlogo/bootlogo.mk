BOOTLOGO_VERSION = 1.0
BOOTLOGO_SITE = $(TOPDIR)/../bootlogo
BOOTLOGO_SITE_METHOD = local
BOOTLOGO_DEPENDENCIES = freetype

define BOOTLOGO_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) CC="$(TARGET_CC)" CFLAGS="-I$(STAGING_DIR)/usr/include/freetype2 -O2" LDFLAGS="-L$(STAGING_DIR)/usr/lib -lfreetype -lm" -C $(@D) all
endef

define BOOTLOGO_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 755 $(@D)/bootlogo $(TARGET_DIR)/usr/bin/bootlogo
endef

$(eval $(generic-package))