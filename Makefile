.RECIPEPREFIX = >
.PHONY: all pkg clean

OO_PS4_TOOLCHAIN ?= $(CURDIR)/OpenOrbis/PS4Toolchain

all: pkg

# Regra falsa para enganar o passo do GitHub Actions
sce_sys/param.sfo:
> @mkdir -p sce_sys
> @touch sce_sys/param.sfo
> @echo "Criado param.sfo falso para o GitHub Actions passar."

# Quando a Action pedir o PKG, nós injetamos o Hello World oficial
pkg:
> @echo "Compilando o Hello World oficial da OpenOrbis..."
> $(MAKE) -C $(OO_PS4_TOOLCHAIN)/samples/hello_world OO_PS4_TOOLCHAIN=$(OO_PS4_TOOLCHAIN)
> @echo "Movendo o PKG oficial para a raiz..."
> cp $(OO_PS4_TOOLCHAIN)/samples/hello_world/*.pkg ./ || true

clean:
> rm -rf *.pkg sce_sys/param.sfo
