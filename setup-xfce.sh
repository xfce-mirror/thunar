#!/usr/bin/bash
version=4.18
apps=(xfce4-dev-tools libxfce4util xfconf libxfce4ui garcon exo)
deps="libgtk-3-dev libglib2.0-dev dbus autoconf automake intltool libjpeg-dev  libexif-dev libnotify-dev libpng-dev tumbler gtk-doc-tools xfce4-dev-tools"
output="output.txt"
sudo apt install $deps
for i in "${apps[@]}"
do
    if [[ $i = "xfce4-dev-tools" ]]; then
        version=4.19
    else
        version=4.18
    fi
    if [[ -e "tarballs/$i.tar.bz2" ]]; then
        echo "$i.tar.bz2 already found. Do you want to re-download? [N/Y]?"
        read ans
        if [[ "$ans" = "N" ]]; then
            continue
        fi
    fi
    echo "Downloading tarball for $i..."
    if [ ! -d tarballs ]; then
        mkdir tarballs/
    fi
    $(curl -L https://archive.xfce.org/src/xfce/$i/$version/$i-$version.0.tar.bz2 --output tarballs/$i.tar.bz2) >> $output
    if [[ -e "$i-$version.0" ]]; then
        echo "$i-$version.0 already found. Do you want to re-install? [N/Y]?"
        read ans
        if [[ "$ans" = "N" ]]; then
            continue
        fi
    fi
    echo "Installing $i..."
    tar -xf tarballs/$i.tar.bz2 >> $output
    cd $i-$version.0
    ./configure >> $output
    make >> $output
    sudo make install >> $output
    cd ../
    echo "Installed $i"
done

## Additional you need to add /usr/local/lib to LD_LIBRARY_PATH if not already added.
## Otherwise thunar won't be able to run
