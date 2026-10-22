"""Build and run product hardware-driver tests on the host.

The PRODUCT_HOST_TEST switch selects explicitly named board mocks. This keeps
pin_mux.h and peripherals.h unique for MCUXpresso Config Tools.
"""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
PLATFORM = ROOT / "_Modules" / "PlatformCore"
PRODUCT = ROOT / "_Product"
DRIVERS = PRODUCT / "Source" / "L01_ProductDriver"
BSP = PRODUCT / "Source" / "L02_BSP"
UNIT = PRODUCT / "Tests" / "Unit"

include_dirs = sorted(
    {path.parent for path in PLATFORM.rglob("*.h")}
    | {path.parent for path in PRODUCT.rglob("*.h")}
    | {ROOT / "board"}
)


def platform_source(name: str) -> Path:
    return next(PLATFORM.rglob(name + ".c"))


def driver_source(name: str) -> Path:
    return DRIVERS / (name + ".c")


suites = {
    "dip_switch": [
        driver_source("product_dip_switch_driver"),
        UNIT / "test_product_dip_switch_driver.c",
    ],
    "rotary_switch": [
        driver_source("product_rotary_switch_driver"),
        UNIT / "test_product_rotary_switch_driver.c",
    ],
    "low_voltage": [
        driver_source("product_low_voltage_driver"),
        UNIT / "test_product_low_voltage_driver.c",
    ],
    "digital_io": [
        platform_source("HalGpio"),
        platform_source("DigitalInputService"),
        platform_source("DigitalOutputService"),
        driver_source("product_digital_io_driver"),
        UNIT / "test_product_digital_io_driver.c",
    ],
    "nvm": [
        platform_source("HalNvm"),
        driver_source("product_nvm_driver"),
        UNIT / "test_product_nvm_driver.c",
    ],
    "dac": [
        platform_source("HalDac"),
        BSP / "bsp_analog_output.c",
        driver_source("product_dac8562_driver"),
        UNIT / "test_product_dac8562_driver.c",
    ],
    "mcu_temperature_injection": [
        driver_source("product_internal_adc_driver"),
        UNIT / "test_product_internal_adc_temperature_injection.c",
    ],
}

with tempfile.TemporaryDirectory(prefix="product-driver-tests-") as directory:
    for name, sources in suites.items():
        output = Path(directory) / name
        command = [
            "gcc",
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-DPRODUCT_HOST_TEST=1",
            *(["-DPRODUCT_SAFETY_TEST_BUILD=1"]
              if name == "mcu_temperature_injection" else []),
            *["-I" + str(path) for path in include_dirs],
            *[str(path) for path in sources],
            "-o",
            str(output),
        ]
        subprocess.run(command, check=True)
        subprocess.run([str(output)], check=True)
        print(f"{name}: PASS", flush=True)
