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
BR2_EXTERNAL_DIR    := $(realpath firmware)
DEFCONFIG_ALL       := $(BR2_EXTERNAL_DIR)/configs/$(BOARD_NAME).defconfig
DEFCONFIG_SDK       := $(BR2_EXTERNAL_DIR)/configs/$(BOARD_NAME)_sdk.defconfig
DEFCONFIG_LINUX     := $(BR2_EXTERNAL_DIR)/board/$(BOARD_NAME)/linux.config
DEFCONFIG_BUSYBOX   := $(BR2_EXTERNAL_DIR)/board/$(BOARD_NAME)/busybox.config
DEFCONFIG_UCLIBC    := $(BR2_EXTERNAL_DIR)/board/$(BOARD_NAME)/uClibc-ng.config
FLASH_SCRIPT        := $(BR2_EXTERNAL_DIR)/board/$(BOARD_NAME)/flash.sh

# === Helpers ===
MKDIR_P  := mkdir -p
RM_F     := rm -f
ECHO     := echo
MAKE_BR  := $(MAKE) -C $(BUILDROOT_DIR)

# -------------------------------------------------------------
# Default target
# -------------------------------------------------------------
.PHONY: all
all: sdk dtb-clean rootfs-clean configure build_all

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
.PHONY: menuconfig savedefconfig sdk-savedefconfig
menuconfig: buildroot
	@$(MAKE_BR) BR2_EXTERNAL=$(BR2_EXTERNAL_DIR) menuconfig

savedefconfig:
# After savedefconfig to rebuild a specified package run: make make <pkg>-rebuild
	@$(MAKE_BR) BR2_DEFCONFIG=$(DEFCONFIG_ALL) savedefconfig

sdk-savedefconfig:
	@$(MAKE_BR) BR2_DEFCONFIG=$(DEFCONFIG_SDK) savedefconfig

# -------------------------------------------------------------
# uclibc config targets
# -------------------------------------------------------------

.PHONY: uclibc-menuconfig uclibc-savedefconfig
uclibc-menuconfig: buildroot
	@$(MAKE_BR) BR2_DEFCONFIG=$(DEFCONFIG_SDK) defconfig
	@$(MAKE_BR) BR2_DEFCONFIG=$(DEFCONFIG_SDK) toolchain
	@$(MAKE_BR) uclibc-menuconfig
	
uclibc-savedefconfig:
	@$(ECHO) "==> Saving uclibc config..."
	@cp $(BUILDROOT_DIR)/output/build/uclibc-*/.config $(DEFCONFIG_UCLIBC)
	$(ECHO) "✔ uclibc config saved to: $(DEFCONFIG_UCLIBC)"

# -------------------------------------------------------------
# Linux Kernel config targets
# -------------------------------------------------------------

.PHONY: linux-menuconfig linux-savedefconfig
linux-menuconfig:
	@$(MAKE_BR) linux-menuconfig
	
linux-savedefconfig:
	@$(ECHO) "==> Saving Linux kernel config..."
	@$(MAKE_BR) linux-savedefconfig
	@cp $(BUILDROOT_DIR)/output/build/linux-*/defconfig $(DEFCONFIG_LINUX)
	$(ECHO) "✔ Linux kernel config saved to: $(DEFCONFIG_LINUX)"

# -------------------------------------------------------------
# BusyBox config targets
# -------------------------------------------------------------
.PHONY: busybox-menuconfig busybox-savedefconfig
busybox-menuconfig:
	@$(MAKE_BR) busybox-menuconfig

busybox-savedefconfig:
	@$(ECHO) "==> Saving BusyBox config..."
	@set -e; \
	BUSYBOX_CONFIG_FILE=$$(find $(BUILDROOT_DIR)/output/build/ -type f -name '.config' -path '*/busybox-*/.config' | head -n1); \
	if [ -z "$$BUSYBOX_CONFIG_FILE" ]; then \
		echo "✖ ERROR: Could not locate BusyBox .config file." >&2; exit 1; \
	else \
		cp "$$BUSYBOX_CONFIG_FILE" $(DEFCONFIG_BUSYBOX); \
		echo "✔ BusyBox config saved to: $(DEFCONFIG_BUSYBOX)"; \
	fi

# -------------------------------------------------------------
# Root filesystem rebuild targets
# -------------------------------------------------------------
.PHONY: target-clean linux-dirclean linux-rebuild dtb-clean dtb-rebuild
target-clean:
	@$(MAKE_BR) target-clean

linux-dirclean:
	@$(MAKE_BR) linux-dirclean

linux-rebuild:
	@$(MAKE_BR) linux-rebuild

dtb-clean:
	@$(ECHO) "==> Deleting custom device tree..."
	@$(RM_F) $(BUILDROOT_DIR)/output/images/stm32f429-disco-custom.dtb
	@$(ECHO) "   ✔ Custom device tree delete complete."

dtb-rebuild: dtb-clean linux-rebuild build_all


.PHONY: rootfs-clean rootfs-rebuild
rootfs-clean:
	@$(ECHO) "==> Deleting root filesystem..."
	@{ \
		[ -d "$(BUILDROOT_DIR)/output/target" ] && rm -rf "$(BUILDROOT_DIR)/output/target" || true; \
		[ -d "$(BUILDROOT_DIR)/output/build" ] && find "$(BUILDROOT_DIR)/output/build" -name '.stamp_target_installed' -delete || true; \
		$(ECHO) "   ✔ Deleted output/target and .stamp_target_installed files"; \
	} || { \
		$(ECHO) "   ✖ Failed to delete rootfs."; exit 1; \
	}

rootfs-rebuild: rootfs-clean
	@$(ECHO) "==> Rebuilding root filesystem only..."
	@{ \
		$(MAKE_BR) && \
		$(ECHO) "   ✔ Root filesystem rebuild complete."; \
	} || { \
		$(ECHO) "   ✖ ERROR: Root filesystem rebuild failed."; exit 1; \
	}

# -------------------------------------------------------------
# Custom target: Always rebuild ioexample1
# -------------------------------------------------------------
.PHONY: ioexample1-rebuild
ioexample1-rebuild:
	@echo "⚠ Forcing rebuild of ioexample1"
	$(MAKE_BR) ioexample1-dirclean
	$(MAKE_BR) ioexample1

# -------------------------------------------------------------
# Custom target: Always rebuild ioexample2
# -------------------------------------------------------------
.PHONY: ioexample2-rebuild
ioexample2-rebuild:
	@echo "⚠ Forcing rebuild of ioexample2"
	$(MAKE_BR) ioexample2-dirclean
	$(MAKE_BR) ioexample2

# -------------------------------------------------------------
# Catch-all: forward unknown targets to Buildroot
# -------------------------------------------------------------
%:
	@$(ECHO) "⚠ Forwarding target '$@' to Buildroot..."
	@$(MAKE_BR) $@
