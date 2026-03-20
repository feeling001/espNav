.PHONY: all build flash dashboard firmware clean check help setup-venv \
        flash-partitions flash-all erase monitor upload uploadfs \
        build-zero build-n16r8 \
        flash-zero flash-n16r8 \
        flash-partitions-zero flash-partitions-n16r8 \
        flash-all-zero flash-all-n16r8

# ─────────────────────────────────────────────────────────────────────────────
# Board selection
#   Default board: esp32s3_zero
#   Override:      make flash BOARD=esp32s3_n16r8
#
# Available boards:
#   esp32s3_zero       ESP32-S3 Zero   4MB Flash / 2MB PSRAM
#   esp32s3_n16r8      ESP32-S3 N16R8 16MB Flash / 8MB PSRAM
# ─────────────────────────────────────────────────────────────────────────────
BOARD ?= esp32s3_zero

# Serial port — override if needed: make flash PORT=/dev/ttyUSB0
PORT  ?= /dev/ttyACM0

# Virtual environment
VENV_DIR = .venv
VENV_BIN = $(VENV_DIR)/bin
PIO      = $(VENV_BIN)/pio

# Build output directory for the selected board
BUILD_DIR = .pio/build/$(BOARD)

# ─────────────────────────────────────────────────────────────────────────────
# Virtual environment setup
# ─────────────────────────────────────────────────────────────────────────────
$(VENV_DIR):
	@echo "Creating Python virtual environment..."
	python3 -m venv $(VENV_DIR)
	$(VENV_BIN)/pip install --upgrade pip
	$(VENV_BIN)/pip install platformio
	@echo "✓ Virtual environment ready"

setup-venv: $(VENV_DIR)

# ─────────────────────────────────────────────────────────────────────────────
# npm dependencies
# ─────────────────────────────────────────────────────────────────────────────
web-dashboard/node_modules:
	@echo "Installing npm dependencies..."
	cd web-dashboard && npm install

install: web-dashboard/node_modules
	@echo "✓ npm dependencies installed"

# ─────────────────────────────────────────────────────────────────────────────
# Dashboard
# ─────────────────────────────────────────────────────────────────────────────
dashboard: web-dashboard/node_modules
	@echo "Building React dashboard..."
	cd web-dashboard && npm run build

# ─────────────────────────────────────────────────────────────────────────────
# Firmware
# ─────────────────────────────────────────────────────────────────────────────
firmware: setup-venv
	@echo "Building firmware for board: $(BOARD)"
	$(PIO) run -e $(BOARD)

build: setup-venv dashboard firmware

# ─────────────────────────────────────────────────────────────────────────────
# Upload targets — all go through PlatformIO
# ─────────────────────────────────────────────────────────────────────────────
upload: setup-venv
	@echo "Uploading firmware (board: $(BOARD))..."
	$(PIO) run -e $(BOARD) -t upload

uploadfs: setup-venv
	@echo "Uploading filesystem (board: $(BOARD))..."
	$(PIO) run -e $(BOARD) -t uploadfs

flash: upload uploadfs

# ─────────────────────────────────────────────────────────────────────────────
# flash-partitions
#   Writes the partition table via PlatformIO's own esptool.
#   Required when changing partition layout between boards.
#   Run 'make firmware BOARD=...' first to generate the .bin file.
# ─────────────────────────────────────────────────────────────────────────────
flash-partitions: setup-venv
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "  Flashing partition table"
	@echo "  Board : $(BOARD)"
	@echo "  Port  : $(PORT)"
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@test -f $(BUILD_DIR)/partitions.bin || \
		(echo "ERROR: $(BUILD_DIR)/partitions.bin not found" && \
		 echo "       Run 'make firmware BOARD=$(BOARD)' first" && exit 1)
	$(PIO) pkg exec -e $(BOARD) -p tool-esptoolpy -- esptool.py \
		--chip esp32s3 --port $(PORT) --baud 921600 \
		write_flash 0x8000 $(BUILD_DIR)/partitions.bin
	@echo "✓ Partition table flashed"

# ─────────────────────────────────────────────────────────────────────────────
# flash-all
#   Full sequence: erase → firmware → filesystem.
#   Uses only PlatformIO targets — no direct esptool calls.
# ─────────────────────────────────────────────────────────────────────────────
flash-all: setup-venv dashboard firmware
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "  Full flash sequence"
	@echo "  Board : $(BOARD)"
	@echo "  Port  : $(PORT)"
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "[1/3] Erasing flash..."
	$(PIO) run -e $(BOARD) -t erase
	@echo "[2/3] Uploading firmware..."
	$(PIO) run -e $(BOARD) -t upload
	@echo "[3/3] Uploading filesystem..."
	$(PIO) run -e $(BOARD) -t uploadfs
	@echo ""
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "✓ Full flash complete (board: $(BOARD))"
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# ─────────────────────────────────────────────────────────────────────────────
# Named aliases — ESP32-S3 Zero (4MB)
# ─────────────────────────────────────────────────────────────────────────────
build-zero:
	$(MAKE) build BOARD=esp32s3_zero

flash-zero:
	$(MAKE) flash BOARD=esp32s3_zero

flash-partitions-zero:
	$(MAKE) flash-partitions BOARD=esp32s3_zero

flash-all-zero:
	$(MAKE) flash-all BOARD=esp32s3_zero

# ─────────────────────────────────────────────────────────────────────────────
# Named aliases — ESP32-S3 N16R8 (16MB)
# ─────────────────────────────────────────────────────────────────────────────
build-n16r8:
	$(MAKE) build BOARD=esp32s3_n16r8

flash-n16r8:
	$(MAKE) flash BOARD=esp32s3_n16r8

flash-partitions-n16r8:
	$(MAKE) flash-partitions BOARD=esp32s3_n16r8

flash-all-n16r8:
	$(MAKE) flash-all BOARD=esp32s3_n16r8

# ─────────────────────────────────────────────────────────────────────────────
# Default target
# ─────────────────────────────────────────────────────────────────────────────
all: setup-venv build flash-all

# ─────────────────────────────────────────────────────────────────────────────
# Monitor / erase
# ─────────────────────────────────────────────────────────────────────────────
monitor: setup-venv
	$(PIO) device monitor

erase: setup-venv
	@echo "Erasing flash (board: $(BOARD))..."
	$(PIO) run -e $(BOARD) -t erase
	@echo "✓ Flash erased"

# ─────────────────────────────────────────────────────────────────────────────
# Clean
# ─────────────────────────────────────────────────────────────────────────────
clean:
	@echo "Cleaning build artifacts..."
	@if [ -d $(VENV_DIR) ]; then $(PIO) run -e $(BOARD) -t clean 2>/dev/null || true; fi
	rm -rf data/www/*
	cd web-dashboard && rm -rf dist/

clean-all: clean
	@echo "Cleaning dependencies..."
	cd web-dashboard && rm -rf node_modules/
	rm -rf $(VENV_DIR)
	@echo "✓ Cleaned venv and dependencies"

# ─────────────────────────────────────────────────────────────────────────────
# Check setup
# ─────────────────────────────────────────────────────────────────────────────
check: setup-venv
	@chmod +x check_setup.sh
	@./check_setup.sh

# ─────────────────────────────────────────────────────────────────────────────
# Help
# ─────────────────────────────────────────────────────────────────────────────
help:
	@echo ""
	@echo "Marine Gateway — Makefile"
	@echo ""
	@echo "Boards:"
	@echo "  esp32s3_zero     ESP32-S3 Zero    4MB Flash / 2MB PSRAM  (default)"
	@echo "  esp32s3_n16r8    ESP32-S3 N16R8  16MB Flash / 8MB PSRAM"
	@echo ""
	@echo "Named targets (no BOARD= needed):"
	@echo "  make build-zero                Build for ESP32-S3 Zero"
	@echo "  make build-n16r8               Build for ESP32-S3 N16R8"
	@echo "  make flash-zero                Flash firmware + filesystem"
	@echo "  make flash-n16r8               Flash firmware + filesystem"
	@echo "  make flash-all-zero            Erase + full flash"
	@echo "  make flash-all-n16r8           Erase + full flash"
	@echo "  make flash-partitions-zero     Flash partition table only"
	@echo "  make flash-partitions-n16r8    Flash partition table only"
	@echo ""
	@echo "Generic targets (use BOARD= to select):"
	@echo "  make build            BOARD=...  Build firmware + dashboard"
	@echo "  make flash            BOARD=...  Upload firmware + filesystem"
	@echo "  make flash-all        BOARD=...  Erase + full flash"
	@echo "  make flash-partitions BOARD=...  Flash partition table only"
	@echo "  make upload           BOARD=...  Upload firmware only"
	@echo "  make uploadfs         BOARD=...  Upload filesystem only"
	@echo "  make firmware         BOARD=...  Build firmware only"
	@echo "  make dashboard                   Build React dashboard only"
	@echo "  make monitor                     Open serial monitor"
	@echo "  make erase            BOARD=...  Erase flash"
	@echo "  make clean            BOARD=...  Clean build artifacts"
	@echo "  make clean-all                   Clean everything incl. venv"
	@echo "  make setup-venv                  Create Python venv + PlatformIO"
	@echo "  make install                     Install npm dependencies"
	@echo "  make check                       Check development environment"
	@echo ""
	@echo "Port override (default: /dev/ttyACM0):"
	@echo "  make flash-all-n16r8 PORT=/dev/ttyUSB0"
	@echo ""



