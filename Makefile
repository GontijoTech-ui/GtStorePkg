.RECIPEPREFIX = >
.PHONY: all pkg clean fix_icon

TARGET     = eboot.bin
CONTENT_ID = UP0000-GTST00001_00-GTSTOREAPP000000

OO_PS4_TOOLCHAIN ?= $(CURDIR)/OpenOrbis/PS4Toolchain
BIN = $(OO_PS4_TOOLCHAIN)/bin/linux

SRCS = $(wildcard source/*.cpp)
OBJS = $(patsubst source/%.cpp,build/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

LIBS = -lc++ -lc -lkernel -lSceNet -lSceSysmodule -lSceHttp -lSceSsl -lSceCommonDialog

CC  = clang --target=x86_64-pc-freebsd12-elf --sysroot=$(OO_PS4_TOOLCHAIN)
CXX = clang++ --target=x86_64-pc-freebsd12-elf --sysroot=$(OO_PS4_TOOLCHAIN)
LD  = ld.lld

CXXFLAGS = -fPIC -funwind-tables -MMD -MP -Wall -Wextra -std=c++11 \
           -isysroot $(OO_PS4_TOOLCHAIN) \
           -isystem $(OO_PS4_TOOLCHAIN)/include \
           -isystem $(OO_PS4_TOOLCHAIN)/include/c++/v1 \
           -Iinclude

all: pkg

pkg: pkg.gp4
> $(BIN)/PkgTool.Core pkg_build pkg.gp4 .

pkg.gp4: $(TARGET) sce_sys/param.sfo fix_icon
> $(BIN)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$(TARGET) sce_sys/param.sfo sce_sys/icon0.png"

fix_icon:
> @echo "Garantindo icon0.png sem canal alfa..."
> convert sce_sys/icon0.png -background black -alpha remove -alpha off PNG24:sce_sys/icon0.png || true

sce_sys/param.sfo: gerar_sfo.py
> python3 gerar_sfo.py --content-id $(CONTENT_ID) --output $@

# Comando puro do FSELF sem flags mutantes que causam o CE-40841-8
$(TARGET): build/source.elf
> $(BIN)/create-fself -in=$< -out=$@

build/source.elf: $(OBJS) | build
> $(LD) -m elf_x86_64 -pie --eh-frame-hdr --script $(OO_PS4_TOOLCHAIN)/link.x -o $@ $(OO_PS4_TOOLCHAIN)/lib/crt1.o $(OBJS) -L$(OO_PS4_TOOLCHAIN)/lib $(LIBS)

build/%.o: source/%.cpp | build
> $(CXX) $(CXXFLAGS) -c -o $@ $<

build:
> @mkdir -p $@

clean:
> rm -rf build $(TARGET) *.pkg pkg.gp4

-include $(DEPS)
