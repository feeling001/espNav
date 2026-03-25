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
#   esp32s3_zero       ESP32-S3 Zero    4MB Flash / 2MB PSRAM  (default)
#   esp32s3_n16r8v     ESP32-S3 N16R8  16MB Flash / 8MB PSRAM
# ─────────────────────────────────────────────────────────────────────────────
BOARD ?= esp32s3_zero

# Serial port — override if needed: make flash PORT=/dev/ttyUSB0
PORT  ?= /dev/ttyACM0

# ─────────────────────────────────────────────────────────────────────────────
# Storage mode
#   STORAGE=progmem   Embed dashboard in firmware binary (single .bin, no uploadfs)
#   STORAGE=littlefs  Serve dashboard from LittleFS filesystem (default)
#
#   Examples:
#     make flash                          → LittleFS mode (default)
#     make flash STORAGE=progmem          → PROGMEM mode
#     make flash-all-n16r8v STORAGE=progmem
# ─────────────────────────────────────────────────────────────────────────────
STORAGE ?= littlefs

# Virtual environment
VENV_DIR = .venv
VENV_BIN = $(VENV_DIR)/bin
PIO      = $(VENV_BIN)/pio

# Build output directory for the selected board
BUILD_DIR = .pio/build/$(BOARD)

# PlatformIO upload/monitor port flag — always passed explicitly
PIO_PORT_FLAG    = --upload-port $(PORT)
PIO_MONITOR_FLAG = --port $(PORT)

# ─────────────────────────────────────────────────────────────────────────────
# Internal helpers
# ─────────────────────────────────────────────────────────────────────────────

# Print a coloured section header
define section
	@echo ""
	@echo "── $(1) ──────────────────────────────────────────────"
endef

# Validate STORAGE value
_check_storage:
	@if [ "$(STORAGE)" != "progmem" ] && [ "$(STORAGE)" != "littlefs" ]; then \
		echo "ERROR: STORAGE must be 'progmem' or 'littlefs' (got '$(STORAGE)')"; \
		exit 1; \
	fi

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
# Dashboard build
#   Always writes to data/www/ (used by both LittleFS uploadfs and the
#   embed_dashboard.py script for PROGMEM embedding).
# ─────────────────────────────────────────────────────────────────────────────
dashboard: web-dashboard/node_modules
	$(call section,Building React dashboard)
	cd web-dashboard && npm run build
	@echo "✓ Dashboard built → data/www/"

# ─────────────────────────────────────────────────────────────────────────────
# Firmware build
#   PROGMEM mode: embed script runs as PlatformIO pre-script automatically.
#   LittleFS mode: same firmware build, no embedding.
#   Either way, dashboard must be built first so data/www/ exists.
# ─────────────────────────────────────────────────────────────────────────────
firmware: setup-venv dashboard _check_storage
	$(call section,Building firmware [BOARD=$(BOARD) STORAGE=$(STORAGE)])
ifeq ($(STORAGE),progmem)
	@echo "Mode: PROGMEM — dashboard embedded in firmware binary"
	$(PIO) run -e $(BOARD)
else
	@echo "Mode: LittleFS — dashboard served from filesystem"
	$(PIO) run -e $(BOARD)
endif

build: firmware

# ─────────────────────────────────────────────────────────────────────────────
# Upload targets
# ─────────────────────────────────────────────────────────────────────────────
upload: setup-venv _check_storage
	$(call section,Uploading firmware [BOARD=$(BOARD) STORAGE=$(STORAGE)])
	$(PIO) run -e $(BOARD) -t upload $(PIO_PORT_FLAG)

# uploadfs is a no-op in PROGMEM mode (dashboard is baked into firmware).
uploadfs: setup-venv _check_storage
ifeq ($(STORAGE),progmem)
	@echo "STORAGE=progmem → uploadfs skipped (dashboard is embedded in firmware)"
else
	$(call section,Uploading filesystem [BOARD=$(BOARD)])
	$(PIO) run -e $(BOARD) -t uploadfs $(PIO_PORT_FLAG)
endif

# flash = upload firmware + upload filesystem (if needed)
flash: upload uploadfs

# ─────────────────────────────────────────────────────────────────────────────
# flash-partitions
# ─────────────────────────────────────────────────────────────────────────────
flash-partitions: setup-venv
	$(call section,Flashing partition table [BOARD=$(BOARD)])
	@test -f $(BUILD_DIR)/partitions.bin || \
		(echo "ERROR: $(BUILD_DIR)/partitions.bin not found" && \
		 echo "       Run 'make firmware BOARD=$(BOARD)' first" && exit 1)
	$(PIO) pkg exec -p tool-esptoolpy -- esptool.py \
		--chip esp32s3 --port $(PORT) --baud 921600 \
		write_flash 0x8000 $(BUILD_DIR)/partitions.bin
	@echo "✓ Partition table flashed"

# ─────────────────────────────────────────────────────────────────────────────
# flash-all — full erase → firmware → filesystem (if needed)
# ─────────────────────────────────────────────────────────────────────────────
flash-all: setup-venv firmware _check_storage
	$(call section,Full flash sequence [BOARD=$(BOARD) STORAGE=$(STORAGE)])
	@echo "[1/$(if $(filter progmem,$(STORAGE)),2,3)] Erasing flash..."
	$(PIO) run -e $(BOARD) -t erase $(PIO_PORT_FLAG)
	@echo "[2/$(if $(filter progmem,$(STORAGE)),2,3)] Uploading firmware..."
	$(PIO) run -e $(BOARD) -t upload $(PIO_PORT_FLAG)
ifeq ($(STORAGE),littlefs)
	@echo "[3/3] Uploading filesystem..."
	$(PIO) run -e $(BOARD) -t uploadfs $(PIO_PORT_FLAG)
endif
	@echo ""
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "✓ Flash complete [BOARD=$(BOARD) STORAGE=$(STORAGE)]"
ifeq ($(STORAGE),progmem)
	@echo "  Dashboard embedded — no uploadfs needed."
else
	@echo "  Dashboard served from LittleFS."
endif
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# ─────────────────────────────────────────────────────────────────────────────
# Named aliases — ESP32-S3 Zero (4MB)
# ─────────────────────────────────────────────────────────────────────────────
build-zero:
	$(MAKE) build BOARD=esp32s3_zero STORAGE=$(STORAGE)

flash-zero:
	$(MAKE) flash BOARD=esp32s3_zero PORT=$(PORT) STORAGE=$(STORAGE)

flash-partitions-zero:
	$(MAKE) flash-partitions BOARD=esp32s3_zero PORT=$(PORT)

flash-all-zero:
	$(MAKE) flash-all BOARD=esp32s3_zero PORT=$(PORT) STORAGE=$(STORAGE)

# ─────────────────────────────────────────────────────────────────────────────
# Named aliases — ESP32-S3 N16R8 (16MB)
# ─────────────────────────────────────────────────────────────────────────────
build-n16r8v:
	$(MAKE) build BOARD=esp32s3_n16r8v STORAGE=$(STORAGE)

flash-n16r8v:
	$(MAKE) flash BOARD=esp32s3_n16r8v PORT=$(PORT) STORAGE=$(STORAGE)

flash-partitions-n16r8v:
	$(MAKE) flash-partitions BOARD=esp32s3_n16r8v PORT=$(PORT)

flash-all-n16r8v:
	$(MAKE) flash-all BOARD=esp32s3_n16r8v PORT=$(PORT) STORAGE=$(STORAGE)

# ─────────────────────────────────────────────────────────────────────────────
# Default target
# ─────────────────────────────────────────────────────────────────────────────
all: setup-venv build flash-all

# ─────────────────────────────────────────────────────────────────────────────
# Monitor
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
	rm -rf src/generated/
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
	@echo "Storage modes (STORAGE=):"
	@echo "  littlefs   Dashboard served from LittleFS filesystem  (default)"
	@echo "             Requires: make upload + make uploadfs"
	@echo "  progmem    Dashboard embedded in firmware binary"
	@echo "             Requires: make upload only — no uploadfs step"
	@echo ""
	@echo "Boards (BOARD=):"
	@echo "  esp32s3_zero     ESP32-S3 Zero    4MB Flash / 2MB PSRAM  (default)"
	@echo "  esp32s3_n16r8v   ESP32-S3 N16R8  16MB Flash / 8MB PSRAM"
	@echo ""
	@echo "Port (default: /dev/ttyACM0):"
	@echo "  All targets accept PORT=/dev/ttyUSB0 (or any path)"
	@echo ""
	@echo "Named targets (board pre-selected, STORAGE= and PORT= still apply):"
	@echo "  make build-zero                        Build for ESP32-S3 Zero"
	@echo "  make build-n16r8v                      Build for ESP32-S3 N16R8"
	@echo "  make flash-zero                        Flash (firmware + fs if needed)"
	@echo "  make flash-n16r8v"
	@echo "  make flash-all-zero                    Erase + full flash"
	@echo "  make flash-all-n16r8v"
	@echo "  make flash-partitions-zero             Flash partition table only"
	@echo "  make flash-partitions-n16r8v"
	@echo ""
	@echo "Generic targets:"
	@echo "  make build            BOARD=... STORAGE=...           Build"
	@echo "  make flash            BOARD=... STORAGE=... PORT=...  Upload fw + fs"
	@echo "  make flash-all        BOARD=... STORAGE=... PORT=...  Erase + full flash"
	@echo "  make flash-partitions BOARD=... PORT=...              Flash partition table"
	@echo "  make upload           BOARD=... PORT=...              Upload firmware only"
	@echo "  make uploadfs         BOARD=... PORT=...              Upload filesystem only"
	@echo "  make firmware         BOARD=... STORAGE=...           Build only"
	@echo "  make dashboard                                        Build React dashboard"
	@echo "  make monitor          BOARD=... PORT=...              Serial monitor"
	@echo "  make erase            BOARD=... PORT=...              Erase flash"
	@echo "  make clean            BOARD=...                       Clean build artifacts"
	@echo "  make clean-all                                        Clean everything"
	@echo "  make setup-venv                                       Create Python venv"
	@echo "  make install                                          Install npm deps"
	@echo "  make check                                            Check environment"
	@echo ""
	@echo "Examples:"
	@echo "  make flash-all-n16r8v STORAGE=progmem PORT=/dev/ttyUSB0"
	@echo "  make flash-all-zero   STORAGE=littlefs PORT=/dev/ttyACM0"
	@echo "  make build            BOARD=esp32s3_zero STORAGE=progmem"
	@echo "  make monitor          PORT=/dev/ttyACM0"
	@echo ""
