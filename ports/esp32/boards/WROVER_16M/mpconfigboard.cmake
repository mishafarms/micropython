set(SDKCONFIG_DEFAULTS
    boards/sdkconfig.base
    boards/sdkconfig.ble
    boards/WROVER_16M/sdkconfig.board
)

set(MICROPY_FROZEN_MANIFEST ${MICROPY_PORT_DIR}/boards/manifest.py)
