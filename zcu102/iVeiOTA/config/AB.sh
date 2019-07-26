#These should come from the u-boot default environment as they will be
#  board specific.  These are just defaults that probably won't work

# MMC device number for the bootable flash part
test -n "${BOOT_DEV}" || setenv BOOT_DEV 99

# MMC partitions numbers for the bootable partitions
test -n "${BOOT_A_PART}" || setenv BOOT_A_PART 99
test -n "${BOOT_B_PART}" || setenv BOOT_B_PART 99

# root filesystem to pass to the kernel in bootargs
test -n "${BOOT_A_ROOT}" || setenv BOOT_A_ROOT invalid
test -n "${BOOT_B_ROOT}" || setenv BOOT_B_ROOT invalid

# How many times to boot a container before giving up
test -n "${BOOT_COUNT_LIMIT}" || setenv BOOT_COUNT_LIMIT 5

# Directory information is stored on bootable partitions
test -n "${boot_path}" || setenv boot_path "/"

# Name and load address in memory of u-boot config file
test -n "${loadbootenv_addr}" || setenv loadbootenv_addr 0x100000
test -n "${bootenv_ab_file}" || setenv bootenv_ab_file uEnvAB.txt

# Name and load address of kernel image
test -n "${kernel_image}" || setenv kernel_image Image
test -n "${kernel_addr}" || setenv kernel_addr 0x80000

# Name and load address of device tree file
test -n "${fdt_file}" || setenv fdt_file fdt.dtb
test -n "${fdt_addr}" || setenv fdt_addr 0x80000

test -n "${force_boot_container}" || setenv force_boot_container none

echo "AB selection script v 1.1.0"

# TODO: Would like to put this into another script so it isn't copy/paste
#first we want to try and load the A settings
echo ${BOOT_DEV}:${BOOT_A_PART} ${boot_path}/${bootenv_ab_file}
if test -e mmc ${BOOT_DEV}:${BOOT_A_PART} ${boot_path}/${bootenv_ab_file}; then
    if test -e mmc ${BOOT_DEV}:${BOOT_A_PART} ${boot_path}/${kernel_image}; then
        if test -e mmc ${BOOT_DEV}:${BOOT_A_PART} ${boot_path}/${fdt_file}; then
            echo "Loading partition A boot info"

            # Clear existing settings
            setenv BOOT_UPDATED
            setenv BOOT_VALID
            setenv BOOT_COUNT
            setenv BOOT_REV

            # Load settings
            load mmc ${BOOT_DEV}:${BOOT_A_PART} ${loadbootenv_addr} ${boot_path}/${bootenv_ab_file}
            env import -t ${loadbootenv_addr} ${filesize}

            # Store settings
            setenv BOOT_A_UPDATED ${BOOT_UPDATED}
            setenv BOOT_A_VALID   ${BOOT_VALID}
            setenv BOOT_A_COUNT   ${BOOT_COUNT}
            setenv BOOT_A_REV     ${BOOT_REV}
            
            setenv BOOT_A_FOUND 1
        else
            echo "Partition A has no device tree file"
            setenv BOOT_A_FOUND 0
        fi
    else
        echo "Partition A has no kernel"
        setenv BOOT_A_FOUND 0
    fi
else
    echo "Partition A boot info not found"
    setenv BOOT_A_FOUND 0
fi

#then load the B settings
echo ${BOOT_DEV}:${BOOT_B_PART} ${boot_path}/${bootenv_ab_file}
if test -e mmc ${BOOT_DEV}:${BOOT_B_PART} ${boot_path}/${bootenv_ab_file}; then
    if test -e mmc ${BOOT_DEV}:${BOOT_B_PART} ${boot_path}/${kernel_image}; then
        if test -e mmc ${BOOT_DEV}:${BOOT_B_PART} ${boot_path}/${fdt_file}; then
            echo "Loading partition B boot info"

            # Clear existing settings
            setenv BOOT_UPDATED
            setenv BOOT_VALID
            setenv BOOT_COUNT
            setenv BOOT_REV

            # Load settings
            load mmc ${BOOT_DEV}:${BOOT_B_PART} ${loadbootenv_addr} ${boot_path}/${bootenv_ab_file}
            env import -t ${loadbootenv_addr} ${filesize}

            # Store settings
            setenv BOOT_B_UPDATED ${BOOT_UPDATED}
            setenv BOOT_B_VALID   ${BOOT_VALID}
            setenv BOOT_B_COUNT   ${BOOT_COUNT}
            setenv BOOT_B_REV     ${BOOT_REV}
            
            setenv BOOT_B_FOUND 1
        else
            echo "Partition B has no device tree file"
            setenv BOOT_B_FOUND 0
        fi
    else
        echo "Partition B has no kernel"
        setenv BOOT_B_FOUND 0
    fi
else
    echo "Partition B boot info not found"
    setenv BOOT_B_FOUND 0
fi

# Set defaults here so that we have valid variables to compare against
#  Make sure that if we use defaults they end up not bootable
test -n "${BOOT_A_UPDATED}" || setenv BOOT_A_UPDATED 0
test -n "${BOOT_A_VALID}"   || setenv BOOT_A_VALID   0
test -n "${BOOT_A_COUNT}"   || setenv BOOT_A_COUNT   100
test -n "${BOOT_A_REV}"     || setenv BOOT_A_REV     0

test -n "${BOOT_B_UPDATED}" || setenv BOOT_B_UPDATED 0
test -n "${BOOT_B_VALID}"   || setenv BOOT_B_VALID   0
test -n "${BOOT_B_COUNT}"   || setenv BOOT_B_COUNT   100
test -n "${BOOT_B_REV}"     || setenv BOOT_B_REV     0

# Check to make sure parition A looks valid
setenv BOOT_A_BOOTABLE 0
setenv BOOT_A_TOO_MANY 0
if test ${BOOT_A_FOUND} = 1; then
    if test ${BOOT_A_VALID} = 1; then
        if test ${BOOT_A_COUNT} -lt ${BOOT_COUNT_LIMIT}; then
            setenv BOOT_A_BOOTABLE 1
        else
            echo "Partition A has too many boot attempts"
            setenv BOOT_A_BOOTABLE 0
            setenv BOOT_A_TOO_MANY 1
        fi
    else
        echo "Partition A is marked invalid"
        setenv BOOT_A_BOOTABLE 0
    fi
fi
   
# Check to make sure parition B looks valid
setenv BOOT_B_BOOTABLE 0
setenv BOOT_B_TOO_MANY 0
if test ${BOOT_B_FOUND} = 1; then
    if test ${BOOT_B_VALID} = 1; then
        if test ${BOOT_B_COUNT} -lt ${BOOT_COUNT_LIMIT}; then
            setenv BOOT_B_BOOTABLE 1
        else
            echo "Partition B has too many boot attempts"
            setenv BOOT_B_BOOTABLE 0
            setenv BOOT_B_TOO_MANY 1
        fi
    else
        echo "Partition B is marked invalid"
        setenv BOOT_B_BOOTABLE 0
    fi
fi

echo "info updated valid count rev bootable"
echo "BOOTA info: ${BOOT_A_UPDATED} ${BOOT_A_VALID} ${BOOT_A_COUNT} ${BOOT_A_REV} ${BOOT_A_BOOTABLE}"
echo "BOOTB info: ${BOOT_B_UPDATED} ${BOOT_B_VALID} ${BOOT_B_COUNT} ${BOOT_B_REV} ${BOOT_B_BOOTABLE}"
setenv bootPartitionA 0
setenv bootPartitionB 0

if test ${force_boot_container} = a; then
    setenv bootPartitionA 1
    echo "Forcing boot of container A";
elif test ${force_boot_container} = b; then
    setenv bootPartitionB 1
    echo "Forcing boot of container B";
elif test ${BOOT_A_BOOTABLE} = 1 && test ${BOOT_B_BOOTABLE} = 0; then
    setenv bootPartitionA 1
    echo "Partition A is bootable and B is not"
elif test ${BOOT_A_BOOTABLE} = 0 && test ${BOOT_B_BOOTABLE} = 1; then
    setenv bootPartitionB 1
    echo "Partition B is bootable and A is not"
elif test ${BOOT_A_BOOTABLE} = 0 && test ${BOOT_B_BOOTABLE} = 0; then
    echo "Neither Partition A or B are bootable"
elif test ${BOOT_A_REV} -gt ${BOOT_B_REV}; then
    setenv bootPartitionA 1
    echo "Partition A has a higher revision"
elif test ${BOOT_A_REV} = ${BOOT_B_REV}; then
    setenv bootPartitionA 1
    echo "Partition A and B have the same revision"
elif test ${BOOT_A_REV} -lt ${BOOT_B_REV}; then
    setenv bootPartitionB 1
    echo "Partition B has a higher revision"
fi

# Check to see if this is a fallback boot
# A fallback boot happens when we would have booted a container, but we don't
#  because its boot count is too high
# TODO: We don't consider it fallback if the other container is marked invalid
#       or can't be found.  May need to revisit that
setenv FALLBACK_BOOT 0
if test ${bootPartitionA} = 1; then
    if test ${BOOT_A_REV} -lt ${BOOT_B_REV}; then
        if test ${BOOT_B_TOO_MANY} = 1; then
            setenv FALLBACK_BOOT 1
        fi
    fi
elif test ${bootPartitionB} = 1; then
    if test ${BOOT_A_REV} -ge ${BOOT_B_REV}; then
        if test ${BOOT_A_TOO_MANY} = 1; then
            setenv FALLBACK_BOOT 1
        fi
    fi
fi

# TODO: Would like to put this into another script so it isn't copy/paste
if test ${bootPartitionA} = 1; then
    echo "Booting partition A"
    
    setenv boot_rootfs ${BOOT_A_ROOT}
    test -n "${defabargs}" && run defabargs
    setenv bootargs ${abbootargs} root=${BOOT_A_ROOT} androidboot.container=a iveia.boot.active=a iveia.boot.alternate=b iveia.boot.rev=${BOOT_A_REV} iveia.boot.updated=${BOOT_A_UPDATED} ievia.boot.fallback=${FALLBACK_BOOT} ${otherargs}
    
    setenv  BOOT_UPDATED ${BOOT_A_UPDATED}
    setenv  BOOT_VALID   ${BOOT_A_VALID}
    setexpr BOOT_COUNT   ${BOOT_A_COUNT} + 1
    setenv  BOOT_REV     ${BOOT_A_REV}

    # Save the updated boot count back to the partition
    env export -t ${loadbootenv_addr} BOOT_UPDATED BOOT_VALID BOOT_COUNT BOOT_REV
    ext4write mmc ${BOOT_DEV}:${BOOT_A_PART} ${loadbootenv_addr} ${boot_path}/${bootenv_ab_file} ${filesize}

    load mmc ${BOOT_DEV}:${BOOT_A_PART} ${fdt_addr} ${boot_path}/${fdt_file}
    test -n ${fdtcommand} && run fdtcommand    
    load mmc ${BOOT_DEV}:${BOOT_A_PART} ${kernel_addr} ${boot_path}/${kernel_image}

    echo "defabargs: << $defabargs >>"
    echo "otherargs: << $otherargs >>"
    echo "bootargs: << $bootargs >>"
    booti $kernel_addr - $fdt_addr    
    
elif test ${bootPartitionB} = 1; then
    echo "Booting partition B"
    
    setenv boot_rootfs ${BOOT_B_ROOT}
    test -n "${defabargs}" && run defabargs
    setenv bootargs ${abbootargs} root=${BOOT_B_ROOT} androidboot.container=b iveia.boot.active=b iveia.boot.alternate=a iveia.boot.rev=${BOOT_B_REV} iveia.boot.updated=${BOOT_B_UPDATED} ievia.boot.fallback=${FALLBACK_BOOT} ${otherargs}
    
    setenv  BOOT_UPDATED ${BOOT_B_UPDATED}
    setenv  BOOT_VALID   ${BOOT_B_VALID}
    setexpr BOOT_COUNT   ${BOOT_B_COUNT} + 1
    setenv  BOOT_REV     ${BOOT_B_REV}

    # Save the updated boot count back to the partition
    env export -t ${loadbootenv_addr} BOOT_UPDATED BOOT_VALID BOOT_COUNT BOOT_REV
    ext4write mmc ${BOOT_DEV}:${BOOT_B_PART} ${loadbootenv_addr} ${boot_path}/${bootenv_ab_file} ${filesize}

    load mmc ${BOOT_DEV}:${BOOT_B_PART} ${fdt_addr} ${boot_path}/${fdt_file}
    test -n ${fdtcommand} && run fdtcommand
    load mmc ${BOOT_DEV}:${BOOT_B_PART} ${kernel_addr} ${boot_path}/${kernel_image}

    echo "defabargs: << $defabargs >>"
    echo "otherargs: << $otherargs >>"
    echo "Bootargs: << $bootargs >>"
    booti $kernel_addr - $fdt_addr    
fi

echo "No partition to boot.  Exiting to u-boot to boot recovery"
exit

