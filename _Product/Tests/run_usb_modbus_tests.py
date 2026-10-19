"""Host RTU stream tests and native SDK syntax checks; no ARM/link validation."""
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET
ROOT = Path(__file__).resolve().parents[2]
platform = ROOT / "_Modules/PlatformCore"
core = platform / "L03_SystemPlatform/Communication/ModbusCore"
includes = ["-I" + str(p) for p in platform.rglob("*") if p.is_dir()]
includes += ["-I" + str(ROOT / "_Product/Source" / name)
             for name in ("L01_ProductDriver", "L03_System")]
includes += ["-I"+str(ROOT/"_Product/Config")]
files = list(core.glob("*.c")) + [
    platform / "L03_SystemPlatform/Communication/ModbusSlave/ModbusSlave.c",
    ROOT / "_Product/Source/L03_System/product_usb_modbus.c",
    ROOT / "_Product/Source/L03_System/product_fw_update.c",
    ROOT / "_Modules/FwUpdateApp/src/fw_update_app.c",
    ROOT / "_Product/Tests/Unit/mocks/mock_fw_update_board.c",
    ROOT / "_Product/Tests/Unit/test_product_usb_modbus.c"]
with tempfile.TemporaryDirectory() as temp:
    binary = str(Path(temp) / "test_usb")
    subprocess.run(["gcc", "-std=c99", "-Wall", "-Wextra", "-Werror"] + includes +
                   [str(p) for p in files] + ["-o", binary], check=True)
    subprocess.run([binary], check=True)
project = ET.parse(ROOT / ".cproject")
for config in project.findall(".//cconfiguration"):
    options = {o.get("superClass"): o for o in config.findall(".//option")}
    paths = []
    for value in options["gnu.c.compiler.option.include.paths"]:
        text = value.get("value").strip('"')
        paths += ["-I" + str(ROOT / text.split("${ProjName}/", 1)[1][:-1])]
    defs = ["-D" + v.get("value") for v in options["gnu.c.compiler.option.preprocessor.def.symbols"]
            if v.get("value") != "__REDLIB__"]
    sdk = list((ROOT / "usb/device").rglob("*.c")) + [
        ROOT / "_Product/Source/L01_ProductDriver/product_usb_cdc.c",
        ROOT / "_Product/Source/L01_ProductDriver/product_usb_osa.c",
        ROOT / "_Product/Source/L01_ProductDriver/usb_device_descriptor.c",
        ROOT / "_Product/Source/L03_System/product_usb_modbus.c",
        ROOT / "_Product/Source/L03_System/product_fw_update.c",
        ROOT / "_Product/Source/L01_ProductDriver/product_fw_update_board.c",
        ROOT / "source/myNXPs16_Prj.c"]
    result = subprocess.run(["gcc", "-fsyntax-only", "-std=gnu99"] + paths + defs +
                            [str(p) for p in sdk], capture_output=True, text=True)
    if result.returncode:
        print(result.stderr)
        raise SystemExit(result.returncode)
    print(config.get("id"), "USB SDK syntax PASS (native SDK warnings expected)")

    with tempfile.TemporaryDirectory() as temp:
        binary = str(Path(temp) / "descriptor")
        result = subprocess.run(["gcc", "-std=gnu99", "-Wno-pointer-to-int-cast",
                                 "-Wno-int-to-pointer-cast"] + paths + defs + ["-UNDEBUG",
            str(ROOT / "_Product/Tests/Unit/test_product_usb_descriptor.c"),
            str(ROOT / "_Product/Source/L01_ProductDriver/usb_device_descriptor.c"),
            "-o", binary], capture_output=True, text=True)
        if result.returncode:
            print(result.stderr)
            raise SystemExit(result.returncode)
        subprocess.run([binary], check=True)
    preprocessed = subprocess.run(["gcc", "-E"] + paths + defs +
        [str(ROOT / "usb/device/class/cdc/usb_device_cdc_acm.c")],
        capture_output=True, text=True, check=True).stdout
    # Declaration is harmless; USB class must emit no debug printf calls.
    assert 'DbgConsole_Printf("' not in preprocessed
    with tempfile.TemporaryDirectory() as temp:
        temp = Path(temp)
        lifecycle = temp / "osa_test.c"
        lifecycle.write_text('''#include "fsl_os_abstraction.h"
#include <assert.h>
#include <string.h>
int main(void) {
    OSA_MUTEX_HANDLE_DEFINE(handle);
    memset(handle, 0xff, sizeof handle);
    assert(OSA_MutexCreate(handle) == KOSA_StatusSuccess);
    for (unsigned i=0; i<sizeof handle; ++i) assert(((unsigned char*)handle)[i]==0);
    memset(handle, 0xff, sizeof handle);
    assert(OSA_MutexDestroy(handle) == KOSA_StatusSuccess);
    for (unsigned i=0; i<sizeof handle; ++i) assert(((unsigned char*)handle)[i]==0);
    assert(OSA_MutexCreate(0) == KOSA_StatusError);
    assert(OSA_MutexDestroy(0) == KOSA_StatusError);
}
''')
        binary = str(temp / "osa_test")
        subprocess.run(["gcc", "-std=gnu99"] + paths + defs + ["-UNDEBUG",
            str(lifecycle), str(ROOT / "_Product/Source/L01_ProductDriver/product_usb_osa.c"),
            "-o", binary], check=True, capture_output=True)
        subprocess.run([binary], check=True)
        asm = temp / "usb_controller.s"
        subprocess.run(["gcc", "-S", "-std=gnu99", "-w"] + paths + defs + [
            str(ROOT / "usb/device/source/lpcip3511/usb_device_lpcip3511.c"),
            "-o", str(asm)], check=True, capture_output=True)
        # Verify actual controller copy buffer and endpoint table use managed USB_RAM BSS.
        code = asm.read_text()
        assert '.bss.$USB_RAM.usb_global' in code
        # SETUP pointer is encoded as address >> 6 by the actual IP3511 driver.
        # It must never be rounded down into a preceding class/device handle.
        align_check = temp / "align_check.c"
        align_check.write_text('''#include "usb_device_config.h"
#include "usb.h"
#if USB_DATA_ALIGN_SIZE < 64
#error IP3511 SETUP buffers must be aligned to 64 bytes
#endif
typedef char control_offset_aligned[(USB_DATA_ALIGN_SIZE_MULTIPLE(8U)%64U)==0U?1:-1];
typedef char zero_offset_aligned[((USB_DATA_ALIGN_SIZE_MULTIPLE(8U)+USB_DATA_ALIGN_SIZE_MULTIPLE(64U))%64U)==0U?1:-1];
typedef char reserve_offset_aligned[((USB_DATA_ALIGN_SIZE_MULTIPLE(8U)+USB_DATA_ALIGN_SIZE_MULTIPLE(64U)+USB_DATA_ALIGN_SIZE_MULTIPLE(4U))%64U)==0U?1:-1];
''')
        subprocess.run(["gcc", "-fsyntax-only"] + paths + defs + [str(align_check)],
                       check=True, capture_output=True)
        for name in ("s_SetupAndEpReservedData", "s_EpCommandStatusList1"):
            before = code[:code.index(name + ":")]
            assert '.bss.$USB_RAM.usb_global' in before[before.rfind('.section'):]
        before = code[:code.index("s_SetupAndEpReservedData:")]
        assert '.align 64' in before[before.rfind('.section'):]
        print("USB OSA lifecycle and controller USB_RAM placement PASS")
