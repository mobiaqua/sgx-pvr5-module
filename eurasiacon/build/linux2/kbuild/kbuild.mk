########################################################################### ###
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Dual MIT/GPLv2
#
# The contents of this file are subject to the MIT license as set out below.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# Alternatively, the contents of this file may be used under the terms of
# the GNU General Public License Version 2 ("GPL") in which case the provisions
# of GPL are applicable instead of those above.
#
# If you wish to allow use of your version of this file only under the terms of
# GPL, and not to allow others to use your version of this file under the terms
# of the MIT license, indicate your decision by deleting the provisions above
# and replace them with the notice and other provisions required by GPL as set
# out in the file called "GPL-COPYING" included in this distribution. If you do
# not delete the provisions above, a recipient may use your version of this file
# under the terms of either the MIT license or GPL.
#
# This License is also included in this distribution in the file called
# "MIT-COPYING".
#
# EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
# PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
# BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
# PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
### ###########################################################################

$(TARGET_PRIMARY_OUT)/kbuild/Makefile: $(MAKE_TOP)/kbuild/Makefile.template
	@[ ! -e $(dir $@) ] && mkdir -p $(dir $@) || true
	$(CP) -f $< $@

# We need to make INTERNAL_KBUILD_MAKEFILES absolute because the files will be
# read while chdir'd into $(KERNELDIR)
INTERNAL_KBUILD_MAKEFILES = $(abspath $(foreach _m,$(KERNEL_COMPONENTS) $(EXTRA_PVRSRVKM_COMPONENTS),$(if $(INTERNAL_KBUILD_MAKEFILE_FOR_$(_m)),$(INTERNAL_KBUILD_MAKEFILE_FOR_$(_m)),$(error Unknown kbuild module "$(_m)"))))
INTERNAL_KBUILD_OBJECTS = $(foreach _m,$(KERNEL_COMPONENTS),$(if $(INTERNAL_KBUILD_OBJECTS_FOR_$(_m)),$(INTERNAL_KBUILD_OBJECTS_FOR_$(_m)),$(error BUG: Unknown kbuild module "$(_m)" should have been caught earlier)))
INTERNAL_EXTRA_KBUILD_OBJECTS = $(foreach _m,$(EXTRA_PVRSRVKM_COMPONENTS),$(if $(INTERNAL_KBUILD_OBJECTS_FOR_$(_m)),$(INTERNAL_KBUILD_OBJECTS_FOR_$(_m)),$(error BUG: Unknown kbuild module "$(_m)" should have been caught earlier)))
.PHONY: kbuild kbuild_clean kbuild_check

kbuild_check:
	@: $(if $(_clang_no_cross_err),$(error $(_clang_no_cross_err_msg)),)
	@: $(if $(strip $(KERNELDIR)),,$(error KERNELDIR must be set))
	@: $(call directory-must-exist,$(KERNELDIR))
	@: $(foreach _m,$(ALL_KBUILD_MODULES),$(if $(wildcard $(abspath $(INTERNAL_KBUILD_MAKEFILE_FOR_$(_m)))),,$(error In makefile $(INTERNAL_MAKEFILE_FOR_MODULE_$(_m)): Module $(_m) requires kbuild makefile $(INTERNAL_KBUILD_MAKEFILE_FOR_$(_m)), which is missing)))
	@: $(if $(filter-out command line override,$(origin build)),,$(error Overriding $$(build) (with "make build=...") will break kbuild))

kbuild: kbuild_check $(TARGET_PRIMARY_OUT)/kbuild/Makefile
	$(if $(V),,@)$(MAKE) -Rr --no-print-directory -C "$(KERNELDIR)" \
		M="$(abspath $(TARGET_PRIMARY_OUT)/kbuild)" \
		INTERNAL_KBUILD_MAKEFILES="$(INTERNAL_KBUILD_MAKEFILES)" \
		INTERNAL_KBUILD_OBJECTS="$(INTERNAL_KBUILD_OBJECTS)" \
		INTERNAL_EXTRA_KBUILD_OBJECTS="$(INTERNAL_EXTRA_KBUILD_OBJECTS)" \
		EXTRA_KBUILD_SOURCE="$(EXTRA_KBUILD_SOURCE)" \
		TARGET_PRIMARY_ARCH=$(TARGET_PRIMARY_ARCH) \
		CROSS_COMPILE="$(CCACHE) $(KERNEL_CROSS_COMPILE)" \
		EXTRA_CFLAGS="$(ALL_KBUILD_CFLAGS)" \
		CC="$(if $(KERNEL_CC),$(KERNEL_CC),$(KERNEL_CROSS_COMPILE)gcc)" \
		AR="$(if $(KERNEL_AR),$(KERNEL_AR),$(KERNEL_CROSS_COMPILE)ar)" \
		LD="$(if $(KERNEL_LD),$(KERNEL_LD),$(KERNEL_CROSS_COMPILE)ld)" \
		NM="$(if $(KERNEL_NM),$(KERNEL_NM),$(KERNEL_CROSS_COMPILE)nm)" \
		OBJCOPY="$(if $(KERNEL_OBJCOPY),$(KERNEL_OBJCOPY),$(KERNEL_CROSS_COMPILE)objcopy)" \
		OBJDUMP="$(if $(KERNEL_OBJDUMP),$(KERNEL_OBJDUMP),$(KERNEL_CROSS_COMPILE)objdump)" \
		V="$(V)" W="$(W)" TOP="$(TOP)"
	@for kernel_module in $(addprefix $(TARGET_PRIMARY_OUT)/kbuild/,$(INTERNAL_KBUILD_OBJECTS:.o=.ko)); do \
		cp $$kernel_module $(TARGET_PRIMARY_OUT); \
	done

kbuild_clean: kbuild_check $(TARGET_PRIMARY_OUT)/kbuild/Makefile
	$(if $(V),,@)$(MAKE) -Rr --no-print-directory -C "$(KERNELDIR)" \
		M="$(abspath $(TARGET_PRIMARY_OUT)/kbuild)" \
		INTERNAL_KBUILD_MAKEFILES="$(INTERNAL_KBUILD_MAKEFILES)" \
		INTERNAL_KBUILD_OBJECTS="$(INTERNAL_KBUILD_OBJECTS)" \
		INTERNAL_EXTRA_KBUILD_OBJECTS="$(INTERNAL_EXTRA_KBUILD_OBJECTS)" \
		EXTRA_KBUILD_SOURCE="$(EXTRA_KBUILD_SOURCE)" \
		TARGET_PRIMARY_ARCH=$(TARGET_PRIMARY_ARCH) \
		CROSS_COMPILE="$(CCACHE) $(KERNEL_CROSS_COMPILE)" \
		EXTRA_CFLAGS="$(ALL_KBUILD_CFLAGS)" \
		CC="$(if $(KERNEL_CC),$(KERNEL_CC),$(KERNEL_CROSS_COMPILE)gcc)" \
		LD="$(if $(KERNEL_LD),$(KERNEL_LD),$(KERNEL_CROSS_COMPILE)ld)" \
		NM="$(if $(KERNEL_NM),$(KERNEL_NM),$(KERNEL_CROSS_COMPILE)nm)" \
		OBJCOPY="$(if $(KERNEL_OBJCOPY),$(KERNEL_OBJCOPY),$(KERNEL_CROSS_COMPILE)objcopy)" \
		V="$(V)" W="$(W)" TOP="$(TOP)" clean

kbuild_install: install
kbuild: install_script_km
