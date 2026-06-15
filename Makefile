#---------------------------------------------------------------------------------
# 3DS Anti-Aliasing Plugin - Makefile (devkitPro / libctru)
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "DEVKITARM not set. Run in Docker: docker run --rm -v "$(CURDIR)":/app -w /app devkitpro/devkitarm make")
endif

include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# Project configuration
#---------------------------------------------------------------------------------
TARGET      := 3ds_aa
BUILD       := build
SOURCES     := source
INCLUDES    := include
DATA        :=
GRAPHICS    :=
ROMFS       :=

#---------------------------------------------------------------------------------
# Library & include paths (CTRULIB & PORTLIBS are set by 3ds_rules)
#---------------------------------------------------------------------------------
LIBDIRS := $(CTRULIB) $(PORTLIBS)

export INCLUDE     := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                      $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                      -I$(CURDIR)/$(BUILD)

export LIBPATHS    := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

#---------------------------------------------------------------------------------
# Build flags
#---------------------------------------------------------------------------------
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS := -g -Wall -Wextra -O2 -mword-relocations \
          -ffunction-sections -fdata-sections \
          -fomit-frame-pointer -ffast-math \
          $(ARCH) $(INCLUDE)

CFLAGS += -D__3DS__

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS := -g $(ARCH)
LDFLAGS  = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS := -lctru -lm

#---------------------------------------------------------------------------------
# Source files
#---------------------------------------------------------------------------------
CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.bin)))

#---------------------------------------------------------------------------------
# Tell make where to find source files (so %.o: %.c works with source/ dir)
#---------------------------------------------------------------------------------
VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))

#---------------------------------------------------------------------------------
# Use standard devkitPro template rules
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
    export LD := $(CC)
else
    export LD := $(CXX)
endif

export OFILES_BIN  := $(addsuffix .o,$(BINFILES))
export OFILES_SRC  := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES      := $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN  := $(addsuffix .h,$(subst .,_,$(BINFILES)))

ifeq ($(strip $(ICON)),)
    icons :=
else
    icons := $(wildcard *.png *.bmp *.jpg)
    ifneq ($(strip $(icons)),)
        export GAME_ICON := $(icons)
    else
        icons :=
    endif
endif

.PHONY: all clean

#---------------------------------------------------------------------------------
# Build targets
#---------------------------------------------------------------------------------
all: $(TARGET).3dsx $(TARGET).elf

$(TARGET).3dsx: $(TARGET).elf
$(TARGET).elf: $(OFILES)

#---------------------------------------------------------------------------------
# Clean
#---------------------------------------------------------------------------------
clean:
	@echo "Cleaning build artifacts..."
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).elf *.o *.d

#---------------------------------------------------------------------------------
# Debug helper - deploy via 3dslink
#---------------------------------------------------------------------------------
run: all
	@echo "Deploying $(TARGET).3dsx to 3DS via 3dslink..."
	3dslink $(TARGET).3dsx

-include $(DEVKITARM)/base_rules
