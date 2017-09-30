#!/system/bin/sh

DATA=/data/touchscreen-reseter/

if [ ! -d $DATA ]; then
    mkdir $DATA
fi

echo "Killing old touchscreen-reseters..."

killall touchscreen-reseter

echo "Remounting system RW..."

mount -o rw,remount /system

echo "Copying files..."

cp config.conf $DATA"config.conf"
cp touchscreen-reseter $DATA"touchscreen-reseter"
cp tsr-init /system/etc/init.d/tsr-init

echo "Setting permissions..."

chmod 755 /system/etc/init.d/tsr-init
chmod 755 $DATA"touchscreen-reseter"

echo "Remounting system RO..."

mount -o ro,remount /system

echo "Starting daemon..."

$DATA"touchscreen-reseter" $DATA"config.conf"

echo "Done."