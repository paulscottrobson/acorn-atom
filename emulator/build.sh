#
#		Build Linux emulator.
#
pushd ../processor
sh build.sh
popd
make -f makefile.linux


