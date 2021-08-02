#!/bin/sh

# README
# this script does configuration + compilation of a new version of kernel, 
# the version is identified by $VERSION

VERSION="SYSCALLS"

OS161VERSION='os161-base-2.0.3'
DIR="/home/os161user/os161/$OS161VERSION"
COMPILEDIR="$DIR/kern/compile"
CONFDIR="$DIR/kern/conf"

cd "$CONFDIR" && ./config "$VERSION" && cd "$COMPILEDIR/$VERSION" && bmake depend && bmake && bmake install
