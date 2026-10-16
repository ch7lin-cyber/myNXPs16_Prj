"""Compile the production watchdog driver against recording hardware stubs."""

from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
DRIVER = ROOT / "_Product/Source/L01_ProductDriver"

with tempfile.TemporaryDirectory(prefix="watchdog-tests-") as directory:
    temp = Path(directory)
    (temp / "fsl_clock.h").write_text(
        "#include <stdint.h>\nuint32_t CLOCK_GetWdtClkFreq(void);\n",
        encoding="utf-8",
    )
    (temp / "fsl_wwdt.h").write_text(
        """
#include <stdint.h>
typedef struct { uint32_t MOD, WINDOW, TC; } WWDT_Type;
extern WWDT_Type mock_wdt;
#define WWDT (&mock_wdt)
#define WWDT_TC_COUNT_MASK 0xFFFFFFU
#define WWDT_WINDOW_WINDOW_MASK 0xFFFFFFU
#define WWDT_MOD_WDEN_MASK 1U
#define WWDT_MOD_WDRESET_MASK 2U
#define WWDT_MOD_WDPROTECT_MASK 16U
void WWDT_SetTimeoutValue(WWDT_Type *, uint32_t);
void WWDT_Refresh(WWDT_Type *);
""",
        encoding="utf-8",
    )
    (temp / "test.c").write_text(
        """
#include <assert.h>
#include <stdint.h>
#include "fsl_wwdt.h"
#include "product_watchdog_driver.h"

WWDT_Type mock_wdt = {3U, 0xFFFFFFU, 2000000U};
static unsigned feeds;
static uint32_t clock_hz = 1000000U;

uint32_t CLOCK_GetWdtClkFreq(void) { return clock_hz; }
void WWDT_SetTimeoutValue(WWDT_Type *base, uint32_t count)
{
    base->TC = count;
}
void WWDT_Refresh(WWDT_Type *base)
{
    assert(base == WWDT);
    ++feeds;
}

int main(void)
{
    unsigned before;
    int index;

    ProductWatchdogDriver_Process(5000U);
    assert(feeds == 0U);
    assert(ProductWatchdogDriver_StartRuntime(10U));
    assert(mock_wdt.TC == 500000U && feeds == 1U);
    ProductWatchdogDriver_Process(1009U);
    assert(feeds == 1U);
    ProductWatchdogDriver_Process(1010U);
    assert(feeds == 2U && g_productWatchdogFeedCount == 1U);
    for (index = 0; index < 100; ++index)
    {
        ProductWatchdogDriver_Process(1010U);
    }
    assert(feeds == 2U);
    ProductWatchdogDriver_Process(6010U);
    assert(feeds == 3U);

    assert(ProductWatchdogDriver_StartRuntime(UINT32_MAX - 499U));
    before = feeds;
    ProductWatchdogDriver_Process(499U);
    assert(feeds == before);
    ProductWatchdogDriver_Process(500U);
    assert(feeds == before + 1U);

    mock_wdt.MOD = 0U;
    assert(!ProductWatchdogDriver_StartRuntime(0U));
    before = feeds;
    ProductWatchdogDriver_Process(1000U);
    assert(feeds == before);
    mock_wdt.MOD = 3U;
    mock_wdt.WINDOW = 100U;
    assert(!ProductWatchdogDriver_StartRuntime(0U));
    mock_wdt.WINDOW = 0xFFFFFFU;
    mock_wdt.MOD |= 16U;
    assert(!ProductWatchdogDriver_StartRuntime(0U));
    mock_wdt.MOD = 3U;
    clock_hz = 0U;
    assert(!ProductWatchdogDriver_StartRuntime(0U));
    return 0;
}
""",
        encoding="utf-8",
    )
    binary = temp / "watchdog_test"
    subprocess.run(
        [
            "gcc",
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I" + str(temp),
            "-I" + str(DRIVER),
            str(DRIVER / "product_watchdog_driver.c"),
            str(temp / "test.c"),
            "-o",
            str(binary),
        ],
        check=True,
    )
    subprocess.run([str(binary)], check=True)

print("PASS: watchdog timing, frozen tick, rollover, and configuration guards")
