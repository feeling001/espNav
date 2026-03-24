.PHONY: all build flash dashboard firmware clean check help setup-venv \
        flash-partitions flash-all erase monitor upload uploadfs \
        build-zero build-n16r8v \
        flash-zero flash-n16r8v \
        flash-partitions-zero flash-partitions-n16r8v \
        flash-all-zero flash-all-n16r8v

# ─────────────────────────────────────────────────────────────────────────────
# Board selection
#   Default board: esp32s3_zero
#   Override:      make flash BOARD=esp32s3_n16r8v
#
# Available boards:
#   esp32s3_zero       ESP32-S3 Zero   4MB Flash / 2MB PSRAM
#   esp32s3_n16r8v     ESP32-S3 N16R8 16MB Flash / 8MB PSRAM
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

# PlatformIO upload/monitor port flag — always passed explicitly
PIO_PORT_FLAG = --upload-port $(PORT)
PIO_MONITOR_FLAG = --port $(PORT)

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
	@echo "Uploading firmware (board: $(BOARD), port: $(PORT))..."
	$(PIO) run -e $(BOARD) -t upload $(PIO_PORT_FLAG)

uploadfs: setup-venv
	@echo "Uploading filesystem (board: $(BOARD), port: $(PORT))..."
	$(PIO) run -e $(BOARD) -t uploadfs $(PIO_PORT_FLAG)

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
	$(PIO) pkg exec -p tool-esptoolpy -- esptool.py \
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
	$(PIO) run -e $(BOARD) -t erase $(PIO_PORT_FLAG)
	@echo "[2/3] Uploading firmware..."
	$(PIO) run -e $(BOARD) -t upload $(PIO_PORT_FLAG)
	@echo "[3/3] Uploading filesystem..."
	$(PIO) run -e $(BOARD) -t uploadfs $(PIO_PORT_FLAG)
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
	$(MAKE) flash BOARD=esp32s3_zero PORT=$(PORT)

flash-partitions-zero:
	$(MAKE) flash-partitions BOARD=esp32s3_zero PORT=$(PORT)

flash-all-zero:
	$(MAKE) flash-all BOARD=esp32s3_zero PORT=$(PORT)

# ─────────────────────────────────────────────────────────────────────────────
# Named aliases — ESP32-S3 N16R8 (16MB)
# ─────────────────────────────────────────────────────────────────────────────
build-n16r8v:
	$(MAKE) build BOARD=esp32s3_n16r8v

flash-n16r8v:
	$(MAKE) flash BOARD=esp32s3_n16r8v PORT=$(PORT)

flash-partitions-n16r8v:
	$(MAKE) flash-partitions BOARD=esp32s3_n16r8v PORT=$(PORT)

flash-all-n16r8v:
	$(MAKE) flash-all BOARD=esp32s3_n16r8v PORT=$(PORT)

# ─────────────────────────────────────────────────────────────────────────────
# Default target
# ─────────────────────────────────────────────────────────────────────────────
all: setup-venv build flash-all

# ─────────────────────────────────────────────────────────────────────────────
# Monitor
#   Always passes --port explicitly so PORT= is respected.
#   Also passes --upload-port so the exception decoder can find the firmware.
# ─────────────────────────────────────────────────────────────────────────────
monitor: setup-venv
	@echo "Opening serial monitor (board: $(BOARD), port: $(PORT))..."
	$(PIO) device monitor -e $(BOARD) $(PIO_MONITOR_FLAG)

# ─────────────────────────────────────────────────────────────────────────────
# Erase
# ─────────────────────────────────────────────────────────────────────────────
erase: setup-venv
	@echo "Erasing flash (board: $(BOARD), port: $(PORT))..."
	$(PIO) run -e $(BOARD) -t erase $(PIO_PORT_FLAG)
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
	@echo "  esp32s3_n16r8v   ESP32-S3 N16R8  16MB Flash / 8MB PSRAM"
	@echo ""
	@echo "Port (default: /dev/ttyACM0):"
	@echo "  All targets accept PORT=/dev/ttyUSB0 (or any path)"
	@echo "  The port is always passed explicitly to PlatformIO."
	@echo ""
	@echo "Named targets (no BOARD= needed):"
	@echo "  make build-zero                Build for ESP32-S3 Zero"
	@echo "  make build-n16r8v              Build for ESP32-S3 N16R8"
	@echo "  make flash-zero                Flash firmware + filesystem"
	@echo "  make flash-n16r8v              Flash firmware + filesystem"
	@echo "  make flash-all-zero            Erase + full flash"
	@echo "  make flash-all-n16r8v          Erase + full flash"
	@echo "  make flash-partitions-zero     Flash partition table only"
	@echo "  make flash-partitions-n16r8v   Flash partition table only"
	@echo ""
	@echo "Generic targets (use BOARD= and PORT= to select):"
	@echo "  make build            BOARD=...           Build firmware + dashboard"
	@echo "  make flash            BOARD=... PORT=...  Upload firmware + filesystem"
	@echo "  make flash-all        BOARD=... PORT=...  Erase + full flash"
	@echo "  make flash-partitions BOARD=... PORT=...  Flash partition table only"
	@echo "  make upload           BOARD=... PORT=...  Upload firmware only"
	@echo "  make uploadfs         BOARD=... PORT=...  Upload filesystem only"
	@echo "  make firmware         BOARD=...           Build firmware only"
	@echo "  make dashboard                            Build React dashboard only"
	@echo "  make monitor          BOARD=... PORT=...  Open serial monitor"
	@echo "  make erase            BOARD=... PORT=...  Erase flash"
	@echo "  make clean            BOARD=...           Clean build artifacts"
	@echo "  make clean-all                            Clean everything incl. venv"
	@echo "  make setup-venv                           Create Python venv + PlatformIO"
	@echo "  make install                              Install npm dependencies"
	@echo "  make check                                Check development environment"
	@echo ""
	@echo "Examples:"
	@echo "  make monitor PORT=/dev/ttyACM0"
	@echo "  make flash-all-n16r8v PORT=/dev/ttyUSB0"
	@echo "  make upload BOARD=esp32s3_zero PORT=/dev/ttyACM1"
	@echo ""
