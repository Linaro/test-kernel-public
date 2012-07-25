#
# linux/arch/arm/tools/Makefile
#
# Copyright (C) 2001 Russell King
#

include/generated/mach-types.h: $(src)/gen-mach-types $(src)/mach-types
	@echo '  Generating $@'
	@mkdir -p $(dir $@)
	$(Q)$(AWK) -f $^ > $@ || { rm -f $@; /bin/false; }

gen_headers:
	$(Q)bash $(srctree)/$(src)/gen-headers $(srctree) $(objtree) $(machine) $(plat)

.PHONY: gen_headers
