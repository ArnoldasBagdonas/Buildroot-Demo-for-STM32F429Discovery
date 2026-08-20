# -------------------------------------------------------------
# Project: Buildroot Automation for STM32F429Discovery Board
# -------------------------------------------------------------

# === Board & Buildroot config ===
BOARD_NAME        := stm32f429disco
BUILDROOT_VERSION             := 2024.02.5
BUILDROOT_VERSION_COMMIT_HASH := 6c084947ab46aa8064947b85ea6168b12d550265
BUILDROOT_DIR     := buildroot

# === SDK config ===
SDK_DIR  := buildroot-sdk
SDK_NAME := arm-buildroot-uclinux-uclibcgnueabi_sdk-buildroot.tar.gz
SDK_ARCHIVE := $(SDK_DIR)/$(SDK_NAME)

# === External customization paths ===
BR2_EXTERNAL_DIR    := $(realpath firmware)
DEFCONFIG_ALL       := $(BR2_EXTERNAL_DIR)/configs/$(BOARD_NAME).defconfig
DEFCONFIG_SDK       := $(BR2_EXTERNAL_DIR)/configs/$(BOARD_NAME)_sdk.defconfig
DEFCONFIG_LINUX     := $(BR2_EXTERNAL_DIR)/board/$(BOARD_NAME)/linux.config
DEFCONFIG_BUSYBOX   := $(BR2_EXTERNAL_DIR)/board/$(BOARD_NAME)/busybox.config
DEFCONFIG_UCLIBC    := $(BR2_EXTERNAL_DIR)/board/$(BOARD_NAME)/uClibc-ng.config
FLASH_SCRIPT        := $(BR2_EXTERNAL_DIR)/board/$(BOARD_NAME)/flash.sh

# Linux-version snapshots. The active Buildroot and Linux files deliberately
# keep version-neutral names; these files are copied in or out by the targets
# in the "Linux Kernel config targets" section below.
LINUX_CONFIG_SNAPSHOT     = $(BR2_EXTERNAL_DIR)/board/$(BOARD_NAME)/linux-$(LINUX_TARGET_VERSION).config
LINUX_DEFCONFIG_SNAPSHOT  = $(BR2_EXTERNAL_DIR)/configs/linux-$(LINUX_TARGET_VERSION)-$(BOARD_NAME).defconfig

# === Helpers ===
MKDIR_P  := mkdir -p
RM_F     := rm -f
ECHO     := echo
# Always override the path cached in buildroot/.config. That configuration may
# have been generated from another checkout location (for example, on the host
# instead of /workspace in the devcontainer), and Buildroot validates the
# cached BR2_EXTERNAL path before it can even run distclean.
MAKE_BR  := $(MAKE) -C $(BUILDROOT_DIR) BR2_EXTERNAL="$(BR2_EXTERNAL_DIR)"

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
	@if [ ! -d "$(BUILDROOT_DIR)" ]; then \
		$(ECHO) "==> Cloning Buildroot ($(BUILDROOT_VERSION))..."; \
		git clone --branch $(BUILDROOT_VERSION) --depth 1 https://github.com/buildroot/buildroot.git $(BUILDROOT_DIR); \
	else \
		if ! CURRENT_HASH=$$(git -c safe.directory="$(abspath $(BUILDROOT_DIR))" -C $(BUILDROOT_DIR) rev-parse HEAD); then \
			$(ECHO) "✖ ERROR: Could not inspect the existing Buildroot checkout." >&2; \
			exit 1; \
		fi; \
		if [ "$$CURRENT_HASH" != "$(BUILDROOT_VERSION_COMMIT_HASH)" ]; then \
			$(ECHO) "==> Buildroot present but incorrect commit ($$CURRENT_HASH ≠ $(BUILDROOT_VERSION_COMMIT_HASH)). Re-cloning..."; \
			rm -rf $(BUILDROOT_DIR); \
			git clone --branch $(BUILDROOT_VERSION) --depth 1 https://github.com/buildroot/buildroot.git $(BUILDROOT_DIR); \
		else \
			$(ECHO) "✔ Buildroot already present and correct version: $(BUILDROOT_VERSION) ($(BUILDROOT_VERSION_COMMIT_HASH))"; \
		fi; \
	fi; \
	$(ECHO) "==> Ensuring downloads and ccache directories exist..."; \
	$(MKDIR_P) $(BUILDROOT_DIR)-downloads $(BUILDROOT_DIR)-ccache; \
	$(ECHO) "✔ Buildroot setup complete."

# -------------------------------------------------------------
# SDK generation
# -------------------------------------------------------------
.PHONY: sdk
sdk: buildroot
	@if [ ! -f "$(SDK_ARCHIVE)" ]; then \
		$(ECHO) "==> Building SDK..."; \
		$(MAKE_BR) BR2_EXTERNAL=$(BR2_EXTERNAL_DIR) BR2_DEFCONFIG=$(DEFCONFIG_SDK) defconfig || exit $$?; \
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
		$(ECHO) "⚠ SDK already exists at: $(SDK_ARCHIVE)"; \
	fi

# -------------------------------------------------------------
# Configuration targets
# -------------------------------------------------------------
.PHONY: configure sdk-configure
configure: buildroot
	@$(MAKE_BR) BR2_EXTERNAL=$(BR2_EXTERNAL_DIR) BR2_DEFCONFIG=$(DEFCONFIG_ALL) defconfig

sdk-configure: buildroot
	@$(MAKE_BR) BR2_EXTERNAL=$(BR2_EXTERNAL_DIR) BR2_DEFCONFIG=$(DEFCONFIG_SDK) defconfig

# -------------------------------------------------------------
# Build and flash targets
# -------------------------------------------------------------
.PHONY: build_all rebuild_all distclean
build_all: sdk
	@$(MAKE_BR)

rebuild_all: buildroot
	@set -eu; \
	if [ ! -s "$(SDK_ARCHIVE)" ]; then \
		$(ECHO) "✖ Missing SDK archive: $(SDK_ARCHIVE)" >&2; \
		$(ECHO) "  Run 'make sdk' once before 'make rebuild_all'." >&2; \
		exit 1; \
	fi; \
	if ! tar -tzf "$(SDK_ARCHIVE)" >/dev/null; then \
		$(ECHO) "✖ Invalid SDK archive: $(SDK_ARCHIVE)" >&2; \
		exit 1; \
	fi; \
	SDK_HASH_BEFORE=$$(sha256sum "$(SDK_ARCHIVE)" | cut -d' ' -f1); \
	$(ECHO) "==> Preserving SDK: $(SDK_ARCHIVE) ($$SDK_HASH_BEFORE)"; \
	$(ECHO) "==> Removing all generated Buildroot output..."; \
	$(MAKE_BR) distclean; \
	$(ECHO) "==> Loading tracked project configuration..."; \
	$(MAKE_BR) BR2_EXTERNAL="$(BR2_EXTERNAL_DIR)" BR2_DEFCONFIG="$(DEFCONFIG_ALL)" defconfig; \
	$(ECHO) "==> Building system from a clean output tree..."; \
	$(MAKE_BR); \
	test -s "$(BUILDROOT_DIR)/output/images/xipImage"; \
	test -s "$(BUILDROOT_DIR)/output/images/stm32f429disco-unified.dtb"; \
	SDK_HASH_AFTER=$$(sha256sum "$(SDK_ARCHIVE)" | cut -d' ' -f1); \
	if [ "$$SDK_HASH_BEFORE" != "$$SDK_HASH_AFTER" ]; then \
		$(ECHO) "✖ SDK archive changed during the system rebuild." >&2; \
		exit 1; \
	fi; \
	$(ECHO) "✔ Clean system rebuild complete; SDK archive unchanged."

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
# After savedefconfig, rebuild a specified package with: make <pkg>-rebuild
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

.PHONY: linux-menuconfig linux-savedefconfig \
	linux-6.1.27-configure linux-6.6.151-configure \
	linux-6.1.27-savedefconfig linux-6.6.151-savedefconfig

linux-6.1.27-configure linux-6.1.27-savedefconfig: LINUX_TARGET_VERSION := 6.1.27
linux-6.6.151-configure linux-6.6.151-savedefconfig: LINUX_TARGET_VERSION := 6.6.151

linux-6.1.27-configure linux-6.6.151-configure: buildroot
	@set -eu; \
	LINUX_CONFIG_SNAPSHOT="$(LINUX_CONFIG_SNAPSHOT)"; \
	LINUX_DEFCONFIG_SNAPSHOT="$(LINUX_DEFCONFIG_SNAPSHOT)"; \
	for SNAPSHOT in "$$LINUX_CONFIG_SNAPSHOT" "$$LINUX_DEFCONFIG_SNAPSHOT"; do \
		if [ ! -f "$$SNAPSHOT" ]; then \
			$(ECHO) "✖ Missing Linux $(LINUX_TARGET_VERSION) snapshot: $$SNAPSHOT" >&2; \
			exit 1; \
		fi; \
	done; \
	if ! grep -Fqx 'BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE="$(LINUX_TARGET_VERSION)"' "$$LINUX_DEFCONFIG_SNAPSHOT"; then \
		$(ECHO) "✖ $$LINUX_DEFCONFIG_SNAPSHOT does not select Linux $(LINUX_TARGET_VERSION)." >&2; \
		exit 1; \
	fi; \
	if ! grep -Fqx 'BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE="$(DEFCONFIG_LINUX)"' "$$LINUX_DEFCONFIG_SNAPSHOT"; then \
		$(ECHO) "✖ $$LINUX_DEFCONFIG_SNAPSHOT must use the active version-neutral linux.config file." >&2; \
		exit 1; \
	fi; \
	cp -f "$$LINUX_CONFIG_SNAPSHOT" "$(DEFCONFIG_LINUX)"; \
	cp -f "$$LINUX_DEFCONFIG_SNAPSHOT" "$(DEFCONFIG_ALL)"; \
	$(ECHO) "==> Restored Linux $(LINUX_TARGET_VERSION) configuration snapshots."; \
	$(MAKE_BR) BR2_DEFCONFIG="$(DEFCONFIG_ALL)" defconfig; \
	$(ECHO) "✔ Linux $(LINUX_TARGET_VERSION) is now the active configured version."

linux-6.1.27-savedefconfig linux-6.6.151-savedefconfig:
	@set -eu; \
	for ACTIVE_CONFIG in "$(DEFCONFIG_LINUX)" "$(DEFCONFIG_ALL)"; do \
		if [ ! -f "$$ACTIVE_CONFIG" ]; then \
			$(ECHO) "✖ Missing active version-neutral config: $$ACTIVE_CONFIG" >&2; \
			exit 1; \
		fi; \
	done; \
	if ! grep -Fqx 'BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE="$(LINUX_TARGET_VERSION)"' "$(DEFCONFIG_ALL)"; then \
		$(ECHO) "✖ Active project defconfig does not select Linux $(LINUX_TARGET_VERSION)." >&2; \
		$(ECHO) "  Run 'make linux-$(LINUX_TARGET_VERSION)-configure' first." >&2; \
		exit 1; \
	fi; \
	if ! grep -Fqx 'BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE="$(DEFCONFIG_LINUX)"' "$(DEFCONFIG_ALL)"; then \
		$(ECHO) "✖ Active project defconfig does not use the version-neutral linux.config file." >&2; \
		exit 1; \
	fi; \
	$(ECHO) "==> Saving the active neutral files as Linux $(LINUX_TARGET_VERSION) snapshots..."; \
	cp -f "$(DEFCONFIG_LINUX)" "$(LINUX_CONFIG_SNAPSHOT)"; \
	cp -f "$(DEFCONFIG_ALL)" "$(LINUX_DEFCONFIG_SNAPSHOT)"; \
	$(ECHO) "✔ Updated Linux $(LINUX_TARGET_VERSION) snapshots:"; \
	$(ECHO) "  $(LINUX_CONFIG_SNAPSHOT)"; \
	$(ECHO) "  $(LINUX_DEFCONFIG_SNAPSHOT)"

linux-menuconfig:
	@$(MAKE_BR) linux-menuconfig
	
linux-savedefconfig:
	@$(ECHO) "==> Saving Linux kernel config..."
	@$(MAKE_BR) linux-update-defconfig
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
	@$(ECHO) "==> Deleting unified device tree..."
	@$(RM_F) $(BUILDROOT_DIR)/output/images/stm32f429disco-unified.dtb
	@$(ECHO) "   ✔ Unified device tree delete complete."

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
# Custom target: Always rebuild hardware tools
# -------------------------------------------------------------
.PHONY: hwtools-rebuild
hwtools-rebuild:
	@echo "⚠ Forcing rebuild of hwtools"
	$(MAKE_BR) hwtools-dirclean
	$(MAKE_BR) hwtools

# -------------------------------------------------------------
# Catch-all: forward unknown targets to Buildroot
# -------------------------------------------------------------
%:
	@$(ECHO) "⚠ Forwarding target '$@' to Buildroot..."
	@$(MAKE_BR) $@
