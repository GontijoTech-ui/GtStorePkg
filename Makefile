TARGET = homebrew.elf
OBJS = build/main.o
CONTENT_ID = IV0002-NPXS39041_00-STOREUPD00000000

# Bibliotecas essenciais do sistema da PS4 e rede
LIBS = -luser_mem_sys -lc++ -lc -lkernel -lSceNet -lSceSysmodule -lSceCommonDialog

OO_PS4_TOOLCHAIN ?= OpenOrbis/PS4Toolchain

CC = clang --target=x86_64-pc-freebsd12-elf --sysroot=$(OO_PS4_TOOLCHAIN)
CXX = clang++ --target=x86_64-pc-freebsd12-elf --sysroot=$(OO_PS4_TOOLCHAIN)
LD = ld.lld

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p build
	$(LD) $(OBJS) -o build/source.elf -m elf_x86_64 -pie --script $(OO_PS4_TOOLCHAIN)/link.x \
		-L$(OO_PS4_TOOLCHAIN)/lib $(LIBS) $(OO_PS4_TOOLCHAIN)/lib/crt1.o
	$(OO_PS4_TOOLCHAIN)/bin/linux/create-fself -in=build/source.elf -out=build/source.oelf --eboot "$(TARGET)" --paid 0x3800000000000011 --authinfo 000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
	
	# Gera o .gp4 e o param.sfo automaticamente com o título definido
	$(OO_PS4_TOOLCHAIN)/bin/linux/create-gp4 -out pkg.gp4 --content-id=$(CONTENT_ID) --title="GTSTORE" --files "$(TARGET)"
	
	# Empacota o projeto gerando o .pkg final
	$(OO_PS4_TOOLCHAIN)/bin/linux/PkgTool.Core pkg_build pkg.gp4 .

build/main.o: source/main.cpp
	@mkdir -p build
	$(CXX) --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c -Wall -std=c++11 \
		-isysroot $(OO_PS4_TOOLCHAIN) \
		-isystem $(OO_PS4_TOOLCHAIN)/include \
		-isystem $(OO_PS4_TOOLCHAIN)/include/c++/v1 \
		-Iinclude -o $@ $<

clean:
	rm -rf build $(TARGET) *.pkg pkg.gp4 param.sfo
