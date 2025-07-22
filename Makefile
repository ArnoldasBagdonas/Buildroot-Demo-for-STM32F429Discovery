# -------------------------------------------------------------
# Project: Buildroot Automation for STM32F429Discovery Board
# -------------------------------------------------------------

# === Board & Buildroot config ===
BOARD_NAME        := stm32f429disco
BUILDROOT_VERSION := 2024.02.5
BUILDROOT_DIR     := buildroot

# === SDK config ===
SDK_DIR  := buildroot-sdk
SDK_NAME := arm-buildroot-uclinux-uclibcgnueabi_sdk-buildroot.tar.gz

# === External customization paths ===
BR2_EXTERNAL_DIR  := $(realpath firmware)
DEFCONFIG_ALL     := $(BR2_EXTERNAL_DIR)/configs/$(BOARD_NAME).defconfig
DEFCONFIG_SDK     := $(BR2_EXTERNAL_DIR)/configs/$(BOARD_NAME)_sdk.defconfig
DEFCONFIG_LINUX   := $(BR2_EXTERNAL_DIR)/board/$(BOARD_NAME)/linux.config
DEFCONFIG_BUSYBOX := $(BR2_EXTERNAL_DIR)/board/$(BOARD_NAME)/busybox.config
FLASH_SCRIPT      := $(BR2_EXTERNAL_DIR)/board/$(BOARD_NAME)/flash.sh

# === Helpers ===
MKDIR_P  := mkdir -p
RM_F     := rm -f
ECHO     := echo
MAKE_BR  := $(MAKE) -C $(BUILDROOT_DIR)

# -------------------------------------------------------------
# Default target
# -------------------------------------------------------------
.PHONY: all
all: sdk configure dtb-clean linux-dirclean build_all

# -------------------------------------------------------------
# Buildroot setup
# -------------------------------------------------------------
.PHONY: buildroot
buildroot:
	@if [ ! -d "$(BUILDROOT_DIR)" ] || [ ! -d "$(BUILDROOT_DIR)/.git" ]; then \
		$(ECHO) "==> Setting up Buildroot ($(BUILDROOT_VERSION))..."; \
		rm -rf $(BUILDROOT_DIR) $(BUILDROOT_DIR)-ccache $(BUILDROOT_DIR)-downloads; \
		git clone --branch $(BUILDROOT_VERSION) --depth 1 https://github.com/buildroot/buildroot.git $(BUILDROOT_DIR); \
	else \
		$(ECHO) "⚠ Buildroot already present and correct version: $(BUILDROOT_VERSION)"; \
	fi; \
	$(ECHO) "==> Ensuring downloads and ccache directories exist..."; \
	$(MKDIR_P) $(BUILDROOT_DIR)-downloads $(BUILDROOT_DIR)-ccache; \
	$(ECHO) "   ✔ Buildroot setup complete."

# -------------------------------------------------------------
# SDK generation
# -------------------------------------------------------------
.PHONY: sdk
sdk: buildroot
	@if [ ! -f "$(SDK_DIR)/$(SDK_NAME)" ]; then \
		$(ECHO) "==> Building SDK..."; \
		$(MAKE_BR) BR2_DEFCONFIG=$(DEFCONFIG_SDK) defconfig || exit $$?; \
		$(MAKE_BR) sdk || exit $$?; \
		if [ -f "$(BUILDROOT_DIR)/output/images/$(SDK_NAME)" ]; then \
			$(MKDIR_P) $(SDK_DIR); \
			cp -f "$(BUILDROOT_DIR)/output/images/$(SDK_NAME)" "$(SDK_DIR)/"; \
			$(ECHO) "   ✔ SDK build complete: $(SDK_DIR)/$(SDK_NAME)"; \
		else \
			$(ECHO) "   ✖ ERROR: SDK file not generated." >&2; \
			exit 1; \
		fi; \
	else \
		$(ECHO) "⚠ SDK already exists at: $(SDK_DIR)/$(SDK_NAME)"; \
	fi

# -------------------------------------------------------------
# Configuration targets
# -------------------------------------------------------------
.PHONY: configure sdk-configure
configure: buildroot
	@$(MAKE_BR) BR2_DEFCONFIG=$(DEFCONFIG_ALL) defconfig

sdk-configure: buildroot
	@$(MAKE_BR) BR2_DEFCONFIG=$(DEFCONFIG_SDK) defconfig

# -------------------------------------------------------------
# Build and flash targets
# -------------------------------------------------------------
.PHONY: build_all rebuild_all distclean
build_all: sdk
	@$(MAKE_BR)

rebuild_all: distclean dtb-clean configure build_all

distclean:
	@$(MAKE_BR) distclean

.PHONY: flash
flash:
	@bash $(FLASH_SCRIPT) $(BUILDROOT_DIR)/output $(BOARD_NAME)

# -------------------------------------------------------------
# Menuconfig and Save Config
# -------------------------------------------------------------
# To create Buildroot configuration from scratch:
# 1) Run `make buildroot` to clone Buildroot if not present
# 2) Enter Buildroot directory: `cd buildroot`
# 3) select initialize project for your board by using configuration form buildroot, in our example `make stm32f429_disco_defconfig`
# 4) Return to project root directory: `cd ..`
# 5) Run `make menuconfig` to open Buildroot configuration menu, configure SDK configuration. Save configuration on exit
# 6) Run `make sdk-savedefconfig` to open Buildroot configuration menu, configure SDK configuration
# 7) Run `make menuconfig` to open Buildroot configuration menu, configure Buildroot project settings:
#    - Customize settings, enable packages, etc.
#    - To enable external SDK integration:
#        * In Buildroot menu, go to "Toolchain" → enable "External Toolchain"
#        * Specify the path to your SDK if needed (e.g. /workspace/$(SDK_DIR)/$(SDK_NAME))
#    - To use and track external Linux kernel config:
#        * Go to "Kernel" → "Use a custom kernel configuration file"
#        * Set the path to your external Linux config file (e.g., $(DEFCONFIG_LINUX))
#        * Make sure this config file is tracked in your repo to keep changes persistent
#    - To use and track external BusyBox config:
#        * Go to "Target packages" → "BusyBox"
#        * Enable "Use a custom BusyBox configuration file"
#        * Set the path to your external BusyBox config file (e.g., $(DEFCONFIG_BUSYBOX))
#        * remove busybox fragment files if any
#    - Save configuration on exit
# 8) Run `make savedefconfig` to save the minimal defconfig file
# 9) Run `make linux-menuconfig`to open Linux configuration menu, customize settings. Save configuration on exit
# 10) Run `make linux-savedefconfig` to save Linux defconfig file outside buildroot tree to be 
# 11) Run `make busybox-menuconfig`to open BusyBox configuration menu, customize settings. Save configuration on exit
# 12) Run `make busybox-savedefconfig` to save BusyBox defconfig file outside buildroot tree to be 
# 13) Commit changes: and external Buildroot/Linux/BusyBox configs to version control

.PHONY: menuconfig savedefconfig sdk-savedefconfig
menuconfig: buildroot
	@$(MAKE_BR) BR2_EXTERNAL=$(BR2_EXTERNAL_DIR) menuconfig

savedefconfig:
	@$(MAKE_BR) BR2_DEFCONFIG=$(DEFCONFIG_ALL) savedefconfig

sdk-savedefconfig:
	@$(MAKE_BR) BR2_DEFCONFIG=$(DEFCONFIG_SDK) savedefconfig

# -------------------------------------------------------------
# Linux Kernel config targets
# -------------------------------------------------------------
# 1) make linux-menuconfig
# 2) make linux-savedefconfig
# 3) make menuconfig
# Kernel  --->
#     [*] Use a custom kernel configuration
#     (/workspace/firmware/board/$(BOARD_NAME)/linux.config)  Custom kernel config file path

.PHONY: linux-menuconfig linux-savedefconfig
linux-menuconfig:
	@$(MAKE_BR) linux-menuconfig

linux-savedefconfig:
	@$(MAKE_BR) linux-savedefconfig
	@cp $(BUILDROOT_DIR)/output/build/linux-*/defconfig $(DEFCONFIG_LINUX)
	@$(ECHO) "Linux kernel config saved to: $(DEFCONFIG_LINUX)"

# -------------------------------------------------------------
# BusyBox config targets
# -------------------------------------------------------------
# 1) make busybox-menuconfig
# 2) make busybox-savedefconfig
# 3) make menuconfig
# Target packages  --->
#     BusyBox  --->
#         [*] Use a custom BusyBox configuration file
#         (/workspace/firmware/board/$(BOARD_NAME)/busybox.config)

.PHONY: busybox-menuconfig busybox-savedefconfig
busybox-menuconfig:
	@$(MAKE_BR) busybox-menuconfig

busybox-savedefconfig:
	@$(ECHO) "Saving BusyBox config..."
	@cp $(BUILDROOT_DIR)/output/build/busybox-*/.config $(DEFCONFIG_BUSYBOX)
	@$(ECHO) "BusyBox config saved to: $(DEFCONFIG_BUSYBOX)"

# -------------------------------------------------------------
# Root filesystem rebuild targets
# -------------------------------------------------------------
.PHONY: linux-dirclean linux-rebuild dtb-clean
linux-dirclean: dtb-clean
	@$(MAKE_BR) linux-dirclean

linux-rebuild: dtb-clean
	@$(MAKE_BR) linux-rebuild

dtb-clean:
	@$(ECHO) "==> Deleting custom device tree..."
	@$(RM_F) $(BUILDROOT_DIR)/output/images/stm32f429-disco-custom.dtb
	@$(ECHO) "   ✔ Custom device tree delete complete."

.PHONY: reinstall-rootfs rebuild-rootfs
reinstall-rootfs:
	@$(ECHO) "==> Reinstalling root filesystem..."
	@{ \
		rm -rf $(BUILDROOT_DIR)/output/target && \
		find $(BUILDROOT_DIR)/output/build/ -name '.stamp_target_installed' -delete && \
		$(ECHO) "   ✔ Cleaned output/target and .stamp_target_installed files"; \
	} || { \
		$(ECHO) "   ✖ Failed to clean rootfs."; exit 1; \
	}

rebuild-rootfs: reinstall-rootfs
	@$(ECHO) "==> Rebuilding root filesystem only..."
	@{ \
		$(MAKE_BR) && \
		$(ECHO) "   ✔ Root filesystem rebuild complete."; \
	} || { \
		$(ECHO) "   ✖ ERROR: Root filesystem rebuild failed."; exit 1; \
	}

# -------------------------------------------------------------
# Catch-all: forward unknown targets to Buildroot
# -------------------------------------------------------------
%:
	@$(ECHO) "Forwarding target '$@' to Buildroot..."
	@$(MAKE_BR) $@
