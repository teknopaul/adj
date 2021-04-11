#!/bin/bash -e
#
# Build a binary .deb package
#
test $(id -u) == "0" || (echo "Run as root" && exit 1) # requires bash -e

#
# The package name
#
name=adj
arch=$(uname -m)

if [ $arch == armv6l ] ; then
  lib_dir=/usr/lib
elif [ $arch == armv7l ] ; then
  lib_dir=/usr/lib
elif [ $arch == x86_64 ] ; then
  lib_dir=/usr/lib
fi


cd $(dirname $0)/..
project_root=$PWD

#
# Create a temporary build directory
#
tmp_dir=/tmp/$name-debbuild
rm -rf $tmp_dir
mkdir -p $tmp_dir/DEBIAN $tmp_dir/usr/bin $tmp_dir/$lib_dir $tmp_dir/etc
chmod 755 $tmp_dir/DEBIAN $tmp_dir/usr/bin $tmp_dir/$lib_dir $tmp_dir/etc

cp --archive target/adj $tmp_dir/usr/bin
cp --archive target/libadj.so $tmp_dir/$lib_dir/libadj.so.1.0
(cd $tmp_dir/$lib_dir; ln -s libadj.so.1.0 libadj.so)
cp --archive etc/* $tmp_dir/etc

size=$(du -sk $tmp_dir | cut -f 1)

. ./version
sed -e "s/@PACKAGE_VERSION@/$VERSION/" $project_root/deploy/DEBIAN/control.in > $tmp_dir/DEBIAN/control
sed -i -e "s/@SIZE@/$size/" $tmp_dir/DEBIAN/control

cp --archive -R $project_root/deploy/DEBIAN/p* $tmp_dir/DEBIAN

#
# setup conffiles
#
(
  cd $tmp_dir/
  find etc -type f | sed 's.^./.' > DEBIAN/conffiles
)

#
# Setup the installation package ownership here if it needs root
#
chown root.root $tmp_dir/usr/bin/*  $tmp_dir/$lib_dir/*
find $tmp_dir/ -type d | xargs chmod 755
find $tmp_dir/ -type f | xargs chmod go-w


#
# Build the .deb
#
dpkg-deb --build $tmp_dir target/$name-$VERSION-1.$arch.deb

test -f target/$name-$VERSION-1.$arch.deb

echo "built target/$name-$VERSION-1.$arch.deb"

if [ -n "$SUDO_USER" ]
then
  chown $SUDO_USER target/ target/$name-$VERSION-1.$arch.deb
fi

# deletes on reboot anyway
# test -d $tmp_dir && rm -rf $tmp_dir
