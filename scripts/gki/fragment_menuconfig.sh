#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2019, The Linux Foundation. All rights reserved.

# Script to edit the kconfig fragments through menuconfig

usage() {
	echo "Usage: $0 <platform_defconfig_variant>"
	echo "Variants: <platform>-gki_defconfig, <platform>-qgki_defconfig, and <platform>-qgki-debug_defconfig"
	echo "Example: $0 lahaina-gki_defconfig"
	exit 1
}

if [ -z "$1" ]; then
	echo "Error: Failed to pass input argument"
	usage
fi

SCRIPTS_ROOT=$(readlink -f $(dirname $0)/)

PLATFORM_NAME=`echo $1 | sed -r "s/(-gki_defconfig|-qgki_defconfig|-qgki-debug_defconfig)$//"`

PLATFORM_NAME=`echo $PLATFORM_NAME | sed "s/vendor\///g"`

# We should be in the kernel root after the envsetup
source ${SCRIPTS_ROOT}/envsetup.sh $PLATFORM_NAME

# Allyes fragment temporarily created on GKI config fragment
QCOM_GKI_ALLYES_FRAG=${CONFIGS_DIR}/${PLATFORM_NAME}_ALLYES_GKI.config

if [ ! -f "${QCOM_GKI_FRAG}" ]; then
	echo "Error: Invalid input"
	usage
fi

REQUIRED_DEFCONFIG=`echo $1 | sed "s/vendor\///g"`

FINAL_DEFCONFIG_BLEND=""

case "$REQUIRED_DEFCONFIG" in
	${PLATFORM_NAME}-qgki-debug_defconfig )
		FINAL_DEFCONFIG_BLEND+=" $QCOM_DEBUG_FRAG"
		;&	# Intentional fallthrough
	${PLATFORM_NAME}-qgki_defconfig )
		FINAL_DEFCONFIG_BLEND+=" $QCOM_QGKI_FRAG"
		${SCRIPTS_ROOT}/fragment_allyesconfig.sh $QCOM_GKI_FRAG $QCOM_GKI_ALLYES_FRAG
		FINAL_DEFCONFIG_BLEND+=" $QCOM_GKI_ALLYES_FRAG "
		;;
	${PLATFORM_NAME}-gki_defconfig )
		FINAL_DEFCONFIG_BLEND+=" $QCOM_GKI_FRAG "
		;&
esac

FINAL_DEFCONFIG_BLEND+=${BASE_DEFCONFIG}

# Reverse the order of the configs for the override to work properly
# Correct order is base_defconfig GKI.config QGKI.config debug.config
FINAL_DEFCONFIG_BLEND=`echo "${FINAL_DEFCONFIG_BLEND}" | awk '{ for (i=NF; i>1; i--) printf("%s ",$i); print $1; }'`

echo "defconfig blend for $REQUIRED_DEFCONFIG: $FINAL_DEFCONFIG_BLEND"

${KERN_SRC}/scripts/kconfig/merge_config.sh $FINAL_DEFCONFIG_BLEND

make savedefconfig
mv defconfig defconfig_base
mv .config .config_base

# Strip off the complete file paths and retail only the values beginning with vendor/
MENUCONFIG_BLEND=""
for config_file in $FINAL_DEFCONFIG_BLEND; do
	if [[ $config_file == *"gki_defconfig" ]]; then
		MENUCONFIG_BLEND+=" "`basename $config_file`" "
	else
		MENUCONFIG_BLEND+=" vendor/"`basename $config_file`" "
	fi
done

# Start the menuconfig
make ${MENUCONFIG_BLEND} menuconfig
make savedefconfig

# The fragment file that we are targeting to edit
FRAG_CONFIG=`echo ${MENUCONFIG_BLEND} | awk 'NF>1{print $NF}' | sed 's/vendor\///'`
FRAG_CONFIG=$CONFIGS_DIR/$FRAG_CONFIG

# CONFIGs to be added
# 'defconfig' file should have been generated.
# Diff this with the 'defconfig_base' from the previous step and extract only the lines that were added
# Finally, remove the "+" from the beginning of the lines and append it to the FRAG_DEFCONFIG
diff -u defconfig_base defconfig | grep "^+CONFIG_" | sed 's/^.//' >> ${FRAG_CONFIG}

# CONFIGs to be removed
configs_to_remove=`diff -u defconfig_base defconfig | grep "^-CONFIG_" | sed 's/^.//'`
for config_del in $configs_to_remove; do
	sed -i "/$config_del/d" ${FRAG_CONFIG}
done

# CONFIGs that are set in base defconfig (or lower fragment), but wanted it to be disabled in FRAG_CONFIG
diff -u .config_base .config | grep "^+# CONFIG_" | sed 's/^.//' >> ${FRAG_CONFIG}

# Cleanup the config files generated during the process
rm -f .config_base .config defconfig defconfig_base

# Cleanup the allyes config fragment that was generated
rm -f $QCOM_GKI_ALLYES_FRAG
