. /lib/functions.sh

REQUIRE_IMAGE_METADATA=1

# copied from x86's platform.sh

platform_check_image() {
	local diskdev partdev diff

	[ "$#" -gt 1 ] && return 1

	export_bootdevice && export_partdevice diskdev 0 || {
		echo "Unable to determine upgrade device"
		return 1
	}

	get_partitions "/dev/$diskdev" bootdisk

	#extract the boot sector from the image
	get_image "$@" | dd of=/tmp/image.bs count=1 bs=512b 2>/dev/null

	get_partitions /tmp/image.bs image

	#compare tables
	diff="$(grep -F -x -v -f /tmp/partmap.bootdisk /tmp/partmap.image)"

	rm -f /tmp/image.bs /tmp/partmap.bootdisk /tmp/partmap.image

	if [ -n "$diff" ]; then
		echo "Partition layout has changed. Full image will be written."
		ask_bool 0 "Abort" && exit 1
		return 0
	fi

	return 0;
}

platform_do_upgrade() {
	local diskdev partdev diff

	export_bootdevice && export_partdevice diskdev 0 || {
		echo "Unable to determine upgrade device"
		return 1
	}

	sync

	if [ "$UPGRADE_OPT_SAVE_PARTITIONS" = "1" ]; then
		get_partitions "/dev/$diskdev" bootdisk

		#extract the boot sector from the image
		get_image "$@" | dd of=/tmp/image.bs count=1 bs=512b

		get_partitions /tmp/image.bs image

		#compare tables
		diff="$(grep -F -x -v -f /tmp/partmap.bootdisk /tmp/partmap.image)"
	else
		diff=1
	fi

	if [ -n "$diff" ]; then
		get_image "$@" | dd of="/dev/$diskdev" bs=2M conv=fsync

		# Separate removal and addtion is necessary; otherwise, partition 1
		# will be missing if it overlaps with the old partition 2
		partx -d - "/dev/$diskdev"
		partx -a - "/dev/$diskdev"

		return 0
	fi

	#iterate over each partition from the image and write it to the boot disk
	while read part start size; do
		if export_partdevice partdev $part; then
			echo "Writing image to /dev/$partdev..."
			get_image "$@" | dd of="/dev/$partdev" ibs="512" obs=1M skip="$start" count="$size" conv=fsync
		else
			echo "Unable to find partition $part device, skipped."
	fi
	done < /tmp/partmap.image

	#copy partition uuid
	echo "Writing new UUID to /dev/$diskdev..."
	get_image "$@" | dd of="/dev/$diskdev" bs=1 skip=440 count=4 seek=440 conv=fsync
}

bcm27xx_set_root_part() {
	local root_part

	if [ -f "/boot/partuuid.txt" ]; then
		root_part="PARTUUID=$(cat "/boot/partuuid.txt")-02"
	else
		root_part="/dev/mmcblk0p2"
	fi

	sed -i "s#\broot=[^ ]*#root=${root_part}#g" "/boot/cmdline.txt"
}

platform_copy_config() {
	local partdev

	if export_partdevice partdev 1; then
		mkdir -p /boot
		[ -f /boot/kernel*.img ] || mount -t vfat -o rw,noatime "/dev/$partdev" /boot

		tar -C / -zxvf "$UPGRADE_BACKUP" boot/cmdline.txt boot/config.txt
		bcm27xx_set_root_part

		local backup_tmp="/tmp/backup-update"
		mkdir -p $backup_tmp
		tar -C $backup_tmp -zxvf $UPGRADE_BACKUP
		cp -af /boot/cmdline.txt $backup_tmp/boot/

		local work_dir=$(pwd)
		cd $backup_tmp
		tar -C $backup_tmp -zcvf /boot/$BACKUP_FILE *
		cd $work_dir

		sync
		umount /boot
	fi
}

platform_restore_backup() {
	local TAR_V=$1

	tar -C / -x${TAR_V}zf "$CONF_RESTORE"
	bcm27xx_set_root_part
}
