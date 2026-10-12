.RECIPEPREFIX = >
.PHONY: all

# Aponta para o toolchain baixado pela sua Action
OO_PS4_TOOLCHAIN ?= $(CURDIR)/OpenOrbis/PS4Toolchain

all:
> @echo "Ignorando os arquivos locais e compilando o Hello World oficial..."
> $(MAKE) -C $(OO_PS4_TOOLCHAIN)/samples/hello_world OO_PS4_TOOLCHAIN=$(OO_PS4_TOOLCHAIN)
> @echo "Movendo o PKG oficial para a raiz para o GitHub Actions capturar..."
> cp $(OO_PS4_TOOLCHAIN)/samples/hello_world/*.pkg ./ || true
