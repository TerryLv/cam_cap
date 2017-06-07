export SDKTARGETSYSROOT=/home/terry/fsl-release-bsp/build/tmp/sysroots/imx6ulevk
export PATH=/home/terry/fsl-release-bsp/build/tmp/sysroots/x86_64-linux/usr/bin/arm-poky-linux-gnueabi:$PATH
export PKG_CONFIG_SYSROOT_DIR=$SDKTARGETSYSROOT
export PKG_CONFIG_PATH=$SDKTARGETSYSROOT/usr/lib/pkgconfig
export OECORE_TARGET_SYSROOT="$SDKTARGETSYSROOT"
#export CC="arm-poky-linux-gnueabi-gcc  -march=armv7-a -mfloat-abi=hard -mfpu=neon -mtune=cortex-a7 --sysroot=$SDKTARGETSYSROOT"
#export CXX="arm-poky-linux-gnueabi-g++  -march=armv7-a -mfloat-abi=hard -mfpu=neon -mtune=cortex-a7 --sysroot=$SDKTARGETSYSROOT"
#export CPP="arm-poky-linux-gnueabi-gcc -E  -march=armv7-a -mfloat-abi=hard -mfpu=neon -mtune=cortex-a7 --sysroot=$SDKTARGETSYSROOT"
export CC="arm-poky-linux-gnueabi-gcc  -march=armv7-a -mfloat-abi=hard -mfpu=neon -mtune=cortex-a9 --sysroot=$SDKTARGETSYSROOT"
export CXX="arm-poky-linux-gnueabi-g++  -march=armv7-a -mfloat-abi=hard -mfpu=neon -mtune=cortex-a9 --sysroot=$SDKTARGETSYSROOT"
export CPP="arm-poky-linux-gnueabi-gcc -E  -march=armv7-a -mfloat-abi=hard -mfpu=neon -mtune=cortex-a9 --sysroot=$SDKTARGETSYSROOT"
export AS="arm-poky-linux-gnueabi-as "
export LD="arm-poky-linux-gnueabi-ld  --sysroot=$SDKTARGETSYSROOT"
export GDB=arm-poky-linux-gnueabi-gdb
export STRIP=arm-poky-linux-gnueabi-strip
export RANLIB=arm-poky-linux-gnueabi-ranlib
export OBJCOPY=arm-poky-linux-gnueabi-objcopy
export OBJDUMP=arm-poky-linux-gnueabi-objdump
export AR=arm-poky-linux-gnueabi-ar
export NM=arm-poky-linux-gnueabi-nm
export M4=m4
export TARGET_PREFIX=arm-poky-linux-gnueabi-
export CONFIGURE_FLAGS="--target=arm-poky-linux-gnueabi --host=arm-poky-linux-gnueabi --build=x86_64-linux --with-libtool-sysroot=$SDKTARGETSYSROOT"
export CFLAGS=" -O2 -pipe -g -feliminate-unused-debug-types"
export CXXFLAGS=" -O2 -pipe -g -feliminate-unused-debug-types"
export LDFLAGS="-Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed"
export CPPFLAGS=""
export KCFLAGS="--sysroot=$SDKTARGETSYSROOT"
export OECORE_DISTRO_VERSION="4.1.15-1.1.0"
export OECORE_SDK_VERSION="4.1.15-1.1.0"
export ARCH=arm
export CROSS_COMPILE=arm-poky-linux-gnueabi-

# Append environment subscripts
if [ -d "$OECORE_TARGET_SYSROOT/environment-setup.d" ]; then
    for envfile in $OECORE_TARGET_SYSROOT/environment-setup.d/*.sh; do
	    source $envfile
    done
fi
if [ -d "$OECORE_NATIVE_SYSROOT/environment-setup.d" ]; then
    for envfile in $OECORE_NATIVE_SYSROOT/environment-setup.d/*.sh; do
	    source $envfile
    done
fi
