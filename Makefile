.RECIPEPREFIX = >
.PHONY: all pkg clean prx

TARGET     = eboot.bin
CONTENT_ID = IV0000-GTST00001_00-GTSTOREAPP000000

# Caminho absoluto. Sobrescreva com: make OO_PS4_TOOLCHAIN=/caminho/PS4Toolchain
OO_PS4_TOOLCHAIN ?= $(CURDIR)/OpenOrbis/PS4Toolchain
BIN = $(OO_PS4_TOOLCHAIN)/bin/linux

# Onde ficam libc.prx e libSceFios2.prx. Se nao existirem la, usa os de sce_module/
PRX_SRC ?= $(OO_PS4_TOOLCHAIN)/sce_module
RIGHT_SRC ?= $(OO_PS4_TOOLCHAIN)/sce_sys/about/right.sprx

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

prx:
> @mkdir -p sce_module sce_sys/about
> @for f in libc.prx libSceFios2.prx; do \
>   if [ -f "$(PRX_SRC)/$$f" ]; then cp -f "$(PRX_SRC)/$$f" sce_module/; fi; \
>   if [ ! -f "sce_module/$$f" ]; then echo "ERRO: sce_module/$$f nao encontrado"; exit 1; fi; \
> done
> @if [ ! -f sce_sys/about/right.sprx ] && [ -f "$(RIGHT_SRC)" ]; then cp -f "$(RIGHT_SRC)" sce_sys/about/right.sprx; fi
> @if [ ! -f sce_sys/about/right.sprx ]; then echo "ERRO: sce_sys/about/right.sprx nao encontrado"; exit 1; fi

pkg.gp4: pkg.gp4.in $(TARGET) sce_sys/param.sfo sce_sys/icon0.png prx Makefile
> sed 's/@CONTENT_ID@/$(CONTENT_ID)/' pkg.gp4.in > $@

sce_sys/param.sfo: gerar_sfo.py Makefile
> python3 gerar_sfo.py --content-id $(CONTENT_ID) --output $@

# -out gera o .oelf e --eboot gera o eboot.bin (antes eram o mesmo arquivo)
$(TARGET): build/source.elf
> $(BIN)/create-fself -in=$< -out=build/source.oelf --eboot "$@" --paid 0x3800000000000011

build/source.elf: $(OBJS) | build
> $(LD) -m elf_x86_64 -pie --eh-frame-hdr --script $(OO_PS4_TOOLCHAIN)/link.x -o $@ $(OO_PS4_TOOLCHAIN)/lib/crt1.o $(OBJS) -L$(OO_PS4_TOOLCHAIN)/lib $(LIBS)

build/%.o: source/%.cpp | build
> $(CXX) $(CXXFLAGS) -c -o $@ $<

build:
> @mkdir -p $@

clean:
> rm -rf build $(TARGET) *.pkg pkg.gp4 sce_sys/param.sfo

-include $(DEPS)
