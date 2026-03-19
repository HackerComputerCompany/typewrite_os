################################################################################
#
# typewrite
#
################################################################################

TYPEWRITE_VERSION = 1.0
TYPEWRITE_SITE = /ironwolf4TB/data01/projects/typewrite_os/typewrite
TYPEWRITE_SITE_METHOD = local
TYPEWRITE_DEPENDENCIES = freetype
TYPEWRITE_INSTALL_TARGET = YES
TYPEWRITE_LICENSE = MIT

define TYPEWRITE_BUILD_CMDS
	$(MAKE) $(TARGET_CONFIGURE_OPTS) \
		CC="$(TARGET_CC)" \
		CFLAGS="-I$(STAGING_DIR)/usr/include/freetype2 -O2" \
		LDFLAGS="-L$(STAGING_DIR)/usr/lib -lfreetype -lm" \
		-C $(TYPEWRITE_SRCDIR) \
		all
endef

define TYPEWRITE_INSTALL_TARGET_CMDS
	install -D -m 755 $(TYPEWRITE_SRCDIR)/typewrite $(TARGET_DIR)/usr/bin/typewrite
endef

$(eval $(generic-package))
