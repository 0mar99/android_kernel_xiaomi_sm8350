# SPDX-License-Identifier: GPL-2.0-only

# auto-detect subdirs
ifeq ($(CONFIG_ARCH_KONA), y)
include $(srctree)/techpack/display/config/konadisp.conf
endif

ifeq ($(CONFIG_ARCH_KONA), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/konadispconf.h
endif

ifneq ($(CONFIG_ARCH_QTI_VM), y)
ifeq ($(CONFIG_ARCH_LAHAINA), y)
     ifeq ($(CONFIG_QGKI), y)
		include $(srctree)/techpack/display/config/lahainadisp.conf
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/lahainadispconf.h
     else
		include $(srctree)/techpack/display/config/gki_lahainadisp.conf
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/gki_lahainadispconf.h
     endif
endif
endif

LINUXINCLUDE    += \
		   -I$(srctree)/techpack/display/include/uapi/display \
		   -I$(srctree)/techpack/display/include
USERINCLUDE     += -I$(srctree)/techpack/display/include/uapi/display

ifeq ($(CONFIG_ARCH_LITO), y)
include $(srctree)/techpack/display/config/saipdisp.conf
endif

ifeq ($(CONFIG_ARCH_LITO), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/saipdispconf.h
endif

ifeq ($(CONFIG_ARCH_BENGAL), y)
include $(srctree)/techpack/display/config/bengaldisp.conf
endif

ifeq ($(CONFIG_ARCH_BENGAL), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/bengaldispconf.h
endif

obj-$(CONFIG_DRM_MSM) += msm/
obj-$(CONFIG_MSM_SDE_ROTATOR) += rotator/
