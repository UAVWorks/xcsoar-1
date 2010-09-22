WARNINGS = -Wall -Wextra
WARNINGS += -Wwrite-strings -Wcast-qual -Wpointer-arith -Wsign-compare
WARNINGS += -Wundef

ifneq ($(TARGET),ANDROID)
WARNINGS += -Wredundant-decls
endif

CXXFLAGS += $(WARNINGS)
CXXFLAGS += -Wmissing-noreturn

# disable some warnings, we're not ready for them yet
CXXFLAGS += -Wno-unused-parameter -Wno-format -Wno-switch -Wno-non-virtual-dtor
#CXXFLAGS += -Wno-missing-field-initializers 

# FastMath.h does dirty aliasing tricks
CXXFLAGS += -Wno-strict-aliasing

# plain C warnings

CFLAGS += $(WARNINGS)
CFLAGS += -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes

# make warnings fatal (for perfectionists)

ifeq ($(WERROR),)
ifneq ($(TARGET),CYGWIN)
WERROR = $(DEBUG)
endif
endif

ifeq ($(WERROR),y)
CXXFLAGS += -Werror
CFLAGS += -Werror
endif

#CXXFLAGS += -pedantic
#CXXFLAGS += -pedantic-errors

# -Wdisabled-optimization
# -Wunused -Wshadow -Wunreachable-code
# -Wfloat-equal
