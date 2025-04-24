#!/bin/sh

# Set the device name
DEVICE="/dev/mmcblk0"

# Use fdisk to perform partition operations
{
    echo d # Delete partition
    echo 2 # Delete partition 2
    echo n # Create a new partition
    echo p # Primary partition
    echo 2 # Partition number 2
    echo 262145 # Start sector
    echo # End sector, use the default value (maximum space)
    echo N # Do not remove the signature
    echo w # Save changes
} | fdisk "$DEVICE"

# Check the file system and expand
resize2fs "${DEVICE}p2"

# Display the disk usage after expansion
df -h
