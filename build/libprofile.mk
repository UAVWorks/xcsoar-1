# Build rules for the profile library

PROFILE_SOURCES = \
	$(SRC)/Profile/ProfileMap.cpp

ifeq ($(HAVE_WIN32),y)
PROFILE_SOURCES += $(SRC)/Profile/Registry.cpp
endif

ifeq ($(USE_GCONF),y)
PROFILE_SOURCES += $(SRC)/Profile/GConf.cpp
endif

PROFILE_OBJS = $(call SRC_TO_OBJ,$(PROFILE_SOURCES))
PROFILE_LIBS = $(TARGET_OUTPUT_DIR)/profile.a
PROFILE_LDLIBS =

ifeq ($(USE_GCONF),y)
PROFILE_LDLIBS += $(GCONF_LDLIBS)
$(PROFILE_OBJS): CPPFLAGS += $(GCONF_CPPFLAGS)
endif

$(PROFILE_LIBS): $(PROFILE_OBJS)
	@$(NQ)echo "  AR      $@"
	$(Q)$(AR) $(ARFLAGS) $@ $^
