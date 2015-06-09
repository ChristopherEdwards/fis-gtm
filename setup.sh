#!/bin/bash
setup-x86_64.exe -q -P wget -P gcc-g++ -P make -P cmake -P libelf-devel -P zlib-devel -P libicu-devel -P libgpgme-devel -P libgpg-error-devel -P libgcrypt-devel -P openssl-devel -P tcsh -P libncurses-devel
mklink /d "c:\cygwin64\home\vagrant\vagrant" "\\vboxsvr\vagrant"
cd ~
wget http://www.hyperrealm.com/libconfig/libconfig-1.5.tar.gz
tar xvzf libconfig-1.5.tar.gz
cd libconfig-1.5
./configure
make
make install
cd ~/vagrant/source/fis-gtm
mkdir build
cd build
locale=$(locale -a | gawk 'BEGIN{IGNORECASE=1}/en_us.utf-*8/{print;exit}')
export LANG=C LC_ALL= LC_CTYPE=$locale LC_COLLATE=C
cmake -D CMAKE_INSTALL_PREFIX:PATH=${PWD}/package ../