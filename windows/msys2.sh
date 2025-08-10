#!/bin/bash -e

USE_CLANG=true
INSTALL_DEPS=false
WITH_GTK=false

usage() { echo "Usage: $0 [--install-deps] [--no-clang] [--with-gtk]" 1>&2; exit 1; }


# read arguments
opts="$(getopt \
	--longoptions "install-deps,no-clang,with-gtk,help" \
	--name "$(basename "$0")" \
	--options "h" \
	-- "$@"
)"

eval set -- "$opts"
unset opts

while true; do
	case $1 in
		--install-deps)
			INSTALL_DEPS=true
			shift 1
			;;

		--no-clang)
			USE_CLANG=false
			shift 1
			;;

		--with-gtk)
			WITH_GTK=true
			shift 1
			;;

		--windows-10-ui)
			WINDOWS_10_UI=true
			shift 1
			;;

		-h|--help)
			usage
			;;

		*)
			break
			;;
	esac
done

if $INSTALL_DEPS; then
	pacman -S --needed --noconfirm\
		mingw-w64-x86_64-toolchain\
		mingw-w64-x86_64-clang\
		mingw-w64-x86_64-lld\
		mingw-w64-x86_64-meson\
		mingw-w64-x86_64-SDL2\
		mingw-w64-x86_64-libpng\
		mingw-w64-x86_64-libepoxy\
		mingw-w64-x86_64-openal

	if $WITH_GTK; then
		pacman -S --needed --noconfirm mingw-w64-x86_64-gtk3
	fi
fi

if $USE_CLANG; then
	export CC=clang CC_LD=lld LDSHARED="clang -shared"
fi

if [ ! -d "build" ]; then
	meson .. build
	# -fPIE isn't supported on windows
	# (clang complains, and gcc generates an executable that silently fails)
	meson configure build -Db_pie=false
fi

if $WITH_GTK; then
	meson configure build -Dgtk=enabled
else
	meson configure build -Dgtk=disabled
fi

# Meson sets -Wl,--allow-shlib-undefined, which isn't recognised by the version
# of lld used by MSYS2. In addition, build.ninja gets regenerated every time
# something changes, so we just allow ninja to fail once here, remove the
# argument, and re-run the build
if $USE_CLANG; then
	ninja -C build || sed -i 's/"-Wl,--allow-shlib-undefined"//' build/build.ninja && ninja -C build
else
	ninja -C build
fi

echo "Compilation complete, copying files..."

mkdir -p dist

files=(
	"build/gbcc.exe"
	"../tileset.png"
	"../camera.png"
	"../print.wav"
	"../LICENSE"
	"../shaders"
	"../icons"
)
if $WITH_GTK; then
	files+=("build/gbcc-gtk.exe" "../src/gtk/gbcc.ui")
	files+=($(ldd build/gbcc-gtk | grep mingw | sed "s/.*=> \([^(]\+\).*/\1/g"))
fi
files+=($(ldd build/gbcc | grep mingw | sed "s/.*=> \([^(]\+\).*/\1/g"))

for f in ${files[@]}; do
	cp -r $f dist
done

cat license_additions.txt >> dist/LICENSE
mv dist/LICENSE{,.txt}

if $WITH_GTK; then
	# The GTK installation process be hella complicated
	cd dist
	mkdir -p share/icons
	cp -r /mingw64/share/icons/Adwaita share/icons/
	cp -r /mingw64/share/icons/hicolor share/icons/

	mkdir -p etc/gtk-3.0
	cp ../settings.ini etc/gtk-3.0

	mkdir -p share/glib-2.0/schemas
	cp /mingw64/share/glib-2.0/schemas/* share/glib-2.0/schemas

	mkdir -p lib
	cp -r /mingw64/lib/gdk-pixbuf-2.0 lib/
fi

printf "\nBuild complete, files are in ./dist"
