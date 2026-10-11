.RECIPEPREFIX = >
TARGET = eboot.bin
OBJS = build/main.o
CONTENT_ID = UP0000-CUSA99999_00-GTSTOREAPP000000
PASSCODE = 00000000000000000000000000000000

LIBS = -luser_mem_sys -lc++ -lc -lkernel -lSceNet -lSceSysmodule -lSceCommonDialog
OO_PS4_TOOLCHAIN ?= OpenOrbis/PS4Toolchain

CC = clang --target=x86_64-pc-freebsd12-elf --sysroot=$(OO_PS4_TOOLCHAIN)
CXX = clang++ --target=x86_64-pc-freebsd12-elf --sysroot=$(OO_PS4_TOOLCHAIN)
LD = ld.lld

all: $(TARGET)

$(TARGET): $(OBJS)
>@mkdir -p build
>$(LD) $(OBJS) -o build/source.elf -m elf_x86_64 -pie --script $(OO_PS4_TOOLCHAIN)/link.x -L$(OO_PS4_TOOLCHAIN)/lib $(LIBS) $(OO_PS4_TOOLCHAIN)/lib/crt1.o
>$(OO_PS4_TOOLCHAIN)/bin/linux/create-fself -in=build/source.elf -out=$(TARGET) --eboot "$(TARGET)" --paid 0x3800000000000011 --authinfo 000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
>$(OO_PS4_TOOLCHAIN)/bin/linux/create-gp4 -out pkg.gp4 --content-id=$(CONTENT_ID) --passcode $(PASSCODE) --files "$(TARGET) sce_sys/param.sfo sce_sys/icon0.png"
>$(OO_PS4_TOOLCHAIN)/bin/linux/PkgTool.Core pkg_build pkg.gp4 .

build/main.o: source/main.cpp
>@mkdir -p build
>$(CXX) --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c -Wall -std=c++11 -isysroot $(OO_PS4_TOOLCHAIN) -isystem $(OO_PS4_TOOLCHAIN)/include -isystem $(OO_PS4_TOOLCHAIN)/include/c++/v1 -Iinclude -o $@ $<

clean:
>rm -rf build $(TARGET) *.pkg pkg.gp4 sce_sys/param.sfo
