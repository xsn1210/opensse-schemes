#!/bin/sh

INSTALL_DIR=$HOME/deps

bold=$(tput bold)
normal=$(tput sgr0)
green=$(tput setaf 2)

if [ -d "$INSTALL_DIR/include/rocksdb" ] && ([ -f "$INSTALL_DIR/lib/librocksdb.a" ] || [ -f "$INSTALL_DIR/lib/librocksdb.so" ]); then
	echo "${bold}${green}RocksDB is already installed${normal}"
else

	echo "${bold}${green}Install RocksDB${normal}"

	set -ex

	git clone --single-branch --depth 1 https://github.com/facebook/rocksdb.git
	cd rocksdb


# Workaround for an issue on Travis
	if [ "$COMPILER" = "clang" ];
	then
		export CC=`which clang`
		export CXX=`which clang++`
	fi

	make INSTALL_PATH=$INSTALL_DIR shared_lib -j4
	sudo -E make INSTALL_PATH=$INSTALL_DIR install-shared
	make clean
fi