human_arch	= ARM (hard float)
build_arch	= arm
header_arch	= arm
defconfig	= defconfig
flavours = linaro-
build_image	= zImage
kernel_file	= arch/$(build_arch)/boot/zImage
install_file	= vmlinuz
no_dumpfile	= true

loader		= grub

skipmodule	= true
skipabi	= true
disable_d_i	= true
do_complete_flavour_headers	= true
do_timestamp_version	= true
