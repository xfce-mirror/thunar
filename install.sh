#!/usr/bin/bash
# v250329

basedir="$(dirname -- "$(readlink -f -- "$0";)")"
opt_clean=0
buildtype="plain"
appname=${0##*/}

usage_exit()
{
    echo "*** usage :"
    echo "$appname clean"
    echo "$appname -type debug"
    echo "$appname -type plain"
    echo "$appname -type release"
    echo "abort..."
    exit 1
}

while (($#)); do
    case "$1" in
        clean)
        opt_clean=1
        ;;
        -help)
        usage_exit
        ;;
        -type)
        shift
        buildtype="$1"
        ;;
        *)
        ;;
    esac
    shift
done

dest=$basedir/build
if [[ $opt_clean == 1 && -d $dest ]]; then
    rm -rf $dest
fi

meson setup build -Dbuildtype=release \
-Dintrospection=false -Dvisibility=false
meson compile -C build
sudo meson install -C build


