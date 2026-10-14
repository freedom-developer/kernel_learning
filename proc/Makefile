obj-m += demo_module.o
demo_module-y := demo.o ../utils/proc.o

KERNEL_BUILD ?= /lib/modules/$(shell uname -r)/build
MODULE_DIR := $(CURDIR)
PROJECT_DIR := $(abspath $(MODULE_DIR)/..)
COMPILE_COMMANDS := $(PROJECT_DIR)/compile_commands.json
GEN_COMPILE_COMMANDS := $(KERNEL_BUILD)/scripts/clang-tools/gen_compile_commands.py
UTIL_PROC_OBJECT := $(PROJECT_DIR)/utils/proc.o
UTIL_PROC_COMMAND := $(PROJECT_DIR)/utils/.proc.o.cmd
ccflags-y += -I$(src)

.PHONY: all clean vscode

all:
	$(MAKE) -C $(KERNEL_BUILD) M=$(MODULE_DIR) modules

vscode: all
	python3 "$(GEN_COMPILE_COMMANDS)" -d "$(KERNEL_BUILD)" \
		-o "$(COMPILE_COMMANDS)" "$(MODULE_DIR)/modules.order"

clean:
	$(MAKE) -C $(KERNEL_BUILD) M=$(MODULE_DIR) clean
	$(RM) "$(UTIL_PROC_OBJECT)" "$(UTIL_PROC_COMMAND)"
