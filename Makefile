#---------------------------------------------------------------------------------
# 3DS Anti-Aliasing Plugin - Makefile (devkitPro / libctru)
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "DEVKITARM not set. Please install devkitPro and run in devkitPro MSYS2 shell.")
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
# Build flags
#---------------------------------------------------------------------------------
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS := -g -Wall -Wextra -O2 -mword-relocations \
          -ffunction-sections -fdata-sections \
          -fomit-frame-pointer -ffast-math \
          $(ARCH) $(INCLUDE)

CFLAGS += $(INCLUDE) -DARM11 -D_3DS

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

export INCLUDE     := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                      $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                      -I$(CURDIR)/$(BUILD)

export LIBPATHS    := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

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
all: $(BUILD) $(TARGET).3dsx $(TARGET).elf

$(BUILD):
	@[ -d $@ ] || mkdir -p $@

$(TARGET).3dsx: $(TARGET).elf
$(TARGET).elf: $(BUILD)/$(TARGET).elf

#---------------------------------------------------------------------------------
# Clean
#---------------------------------------------------------------------------------
clean:
	@echo "Cleaning build artifacts..."
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).elf

#---------------------------------------------------------------------------------
# Debug helper - deploy via 3dslink
#---------------------------------------------------------------------------------
run: all
	@echo "Deploying $(TARGET).3dsx to 3DS via 3dslink..."
	3dslink $(TARGET).3dsx

-include $(DEVKITARM)/base_rules
