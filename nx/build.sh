#!/bin/sh
# Compila gow3-nx. Make de libnx no soporta espacios en la ruta ("Ports Swtch"),
# así que se copia el fuente a una carpeta espejo sin espacios en %TEMP%, se compila
# ahí (build/ queda en el espejo, incremental) y se traen el .nro y el .elf.
# Uso (Git Bash): sh "D:/Ports Swtch/GOD OF WAR/nx/build.sh" [objetivo-make]
SRC="/d/Ports Swtch/GOD OF WAR/nx"
M="${TMPDIR:-/tmp}/gow3-nx-build"
mkdir -p "$M"
cp -ru "$SRC/Makefile" "$SRC/source" "$SRC/include" "$M/"
[ -d "$SRC/romfs" ] && cp -ru "$SRC/romfs" "$M/"
export DEVKITPRO=/opt/devkitpro
/c/devkitPro/msys2/usr/bin/bash.exe -lc "cd $M && make $*" || exit $?
cp -f "$M"/gow3-nx.nro "$M"/gow3-nx.elf "$SRC/" 2>/dev/null
exit 0
