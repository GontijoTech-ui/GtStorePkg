.RECIPEPREFIX = >
.PHONY: all pkg clean

TARGET     = eboot.bin
CONTENT_ID = UP0000-GTST00001_00-GTSTOREAPP000000

OO_PS4_TOOLCHAIN ?= $(CURDIR)/OpenOrbis/PS4Toolchain
BIN = $(OO_PS4_TOOLCHAIN)/bin/linux

# ISOLAMENTO: Vamos compilar APENAS o main.cpp para descartar o proxy
SRCS = source/main.cpp
OBJS = $(patsubst source/%.cpp,build/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

# ISOLAMENTO: Removemos SceHttp, SceSsl e SceCommonDialog
LIBS = -lc++ -lc -lkernel -lSceNet -lSceSysmodule

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

pkg.gp4: $(TARGET) sce_sys/param.sfo sce_sys/icon0.png
> $(BIN)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$(TARGET) sce_sys/param.sfo sce_sys/icon0.png"

sce_sys/param.sfo: gerar_sfo.py
> python3 gerar_sfo.py --content-id $(CONTENT_ID) --output $@

# Restaurada a flag --eboot que é obrigatória para o conversor saber o que gerar
$(TARGET): build/source.elf
> $(BIN)/create-fself -in=$< --eboot $@ --paid 0x3800000000000011

build/source.elf: $(OBJS) | build
> $(LD) -m elf_x86_64 -pie --eh-frame-hdr --script $(OO_PS4_TOOLCHAIN)/link.x -o $@ $(OO_PS4_TOOLCHAIN)/lib/crt1.o $(OBJS) -L$(OO_PS4_TOOLCHAIN)/lib $(LIBS)
> @echo "=== DEPENDENCIAS DO ELF (NEEDED) ==="
> readelf -d $@ | grep NEEDED || true
> @echo "===================================="

build/%.o: source/%.cpp | build
> $(CXX) $(CXXFLAGS) -c -o $@ $<

build:
> @mkdir -p $@

clean:
> rm -rf build $(TARGET) *.pkg pkg.gp4

-include $(DEPS)
