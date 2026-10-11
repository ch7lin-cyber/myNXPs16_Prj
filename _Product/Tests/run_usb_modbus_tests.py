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
files = list(core.glob("*.c")) + [
    platform / "L03_SystemPlatform/Communication/ModbusSlave/ModbusSlave.c",
    ROOT / "_Product/Source/L03_System/product_usb_modbus.c",
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
        ROOT / "_Product/Source/L01_ProductDriver/usb_device_descriptor.c",
        ROOT / "_Product/Source/L03_System/product_usb_modbus.c",
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
