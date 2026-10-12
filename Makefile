.RECIPEPREFIX = >
.PHONY: all pkg clean prx

TARGET     = eboot.bin
CONTENT_ID = IV0000-GTST00001_00-GTSTOREAPP000000

# Caminho absoluto. Sobrescreva com: make OO_PS4_TOOLCHAIN=/caminho/PS4Toolchain
OO_PS4_TOOLCHAIN ?= $(CURDIR)/OpenOrbis/PS4Toolchain
BIN = $(OO_PS4_TOOLCHAIN)/bin/linux

# Onde ficam libc.prx e libSceFios2.prx. Se o seu toolchain guarda em outro lugar,
# sobrescreva: make PRX_SRC=/outro/caminho
PRX_SRC ?= $(OO_PS4_TOOLCHAIN)/sce_module

# Teste de empacotamento: make MINIMAL=1 gera um app que so fica dormindo.
ifeq ($(MINIMAL),1)
EXTRA_FLAGS = -DGT_MINIMAL
endif

SRCS = $(wildcard source/*.cpp)
OBJS = $(patsubst source/%.cpp,build/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

LIBS = -lc++ -lc -lkernel -lSceNet -lSceSysmodule -lSceHttp -lSceSsl

CC  = clang --target=x86_64-pc-freebsd12-elf --sysroot=$(OO_PS4_TOOLCHAIN)
CXX = clang++ --target=x86_64-pc-freebsd12-elf --sysroot=$(OO_PS4_TOOLCHAIN)
LD  = ld.lld

CXXFLAGS = -fPIC -funwind-tables -MMD -MP -Wall -Wextra -std=c++11 $(EXTRA_FLAGS) \
           -isysroot $(OO_PS4_TOOLCHAIN) \
           -isystem $(OO_PS4_TOOLCHAIN)/include \
           -isystem $(OO_PS4_TOOLCHAIN)/include/c++/v1 \
           -Iinclude

all: pkg

pkg: pkg.gp4
> $(BIN)/PkgTool.Core pkg_build pkg.gp4 .

# Copia libc.prx e libSceFios2.prx do toolchain (o sample do OpenOrbis empacota os dois).
prx:
> @mkdir -p sce_module
> @for f in libc.prx libSceFios2.prx; do \
>   if [ -f "$(PRX_SRC)/$$f" ]; then cp -f "$(PRX_SRC)/$$f" sce_module/; \
>   else echo "AVISO: $(PRX_SRC)/$$f nao encontrado (use PRX_SRC=...)"; fi; \
> done

pkg.gp4: $(TARGET) sce_sys/param.sfo sce_sys/icon0.png prx
> $(BIN)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$(TARGET) sce_sys/param.sfo sce_sys/icon0.png $$(ls sce_module/*.prx 2>/dev/null | tr '\n' ' ')"

sce_sys/param.sfo: gerar_sfo.py Makefile
> python3 gerar_sfo.py --content-id $(CONTENT_ID) --output $@

# create-fself: -out gera o .oelf e --eboot gera o eboot.bin. Eram o MESMO arquivo
# antes (eboot.bin), o que pode corromper o eboot. Agora sao arquivos diferentes.
$(TARGET): build/source.elf
> $(BIN)/create-fself -in=$< -out=build/source.oelf --eboot "$@" --paid 0x3800000000000011

build/source.elf: $(OBJS) | build
> $(LD) -m elf_x86_64 -pie --eh-frame-hdr --script $(OO_PS4_TOOLCHAIN)/link.x -o $@ $(OO_PS4_TOOLCHAIN)/lib/crt1.o $(OBJS) -L$(OO_PS4_TOOLCHAIN)/lib $(LIBS)

build/%.o: source/%.cpp | build
> $(CXX) $(CXXFLAGS) -c -o $@ $<

build:
> @mkdir -p $@

clean:
> rm -rf build sce_module $(TARGET) *.pkg pkg.gp4 sce_sys/param.sfo

-include $(DEPS)
