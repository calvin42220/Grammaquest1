#!/bin/bash
# Compile le jeu en NatureDesMots.nds (a lancer dans l'image devkitpro/devkitarm)
set -ex
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export DEVKITARM=${DEVKITARM:-$DEVKITPRO/devkitARM}
export PATH="$DEVKITARM/bin:$DEVKITPRO/tools/bin:$PATH"

# libnds recent = libnds + calico (d'ou les deux dossiers include/lib et ds9.specs)
arm-none-eabi-gcc -O2 -Wall \
  -march=armv5te -mtune=arm946e-s \
  -D__NDS__ -DARM9 \
  -I"$DEVKITPRO/calico/include" -I"$DEVKITPRO/libnds/include" \
  -specs="$DEVKITPRO/calico/share/ds9.specs" \
  source/*.c \
  -L"$DEVKITPRO/libnds/lib" -L"$DEVKITPRO/calico/lib" \
  -lfat -lnds9 -lcalico_ds9 \
  -o NatureDesMots.elf

# Programme ARM7 par defaut fourni avec calico (gere stylet, son, boutons...)
find "$DEVKITPRO/calico" "$DEVKITPRO/libnds" -name 'ds7*.elf' -o -name 'default*.elf' || true
ARM7="$DEVKITPRO/calico/bin/ds7_maine.elf"
if [ ! -f "$ARM7" ]; then ARM7=$(find "$DEVKITPRO" -name 'ds7*.elf' | head -n 1); fi
echo "ARM7 utilise : $ARM7"

ndstool -c NatureDesMots.nds -7 "$ARM7" -9 NatureDesMots.elf

ls -l NatureDesMots.nds
