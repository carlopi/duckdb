#!/usr/bin/env bash

set -ex

cd tools/nodejs
./configure

export TAG='aaaaaaaargh'
# for master do prereleases
export VER=`something`
export DIST=`somethingsomething`

# set version to lastver
npm version $VER
npm version prerelease --preid="dev"$DIST
export TAG='--tag next'

npm pack --dry-run
