"""생성 헤더의 include-free 계약과 MSVC C++20 소비자를 독립 임시 출력에서 검증한다."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET
import zipfile


SHEET_NS = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"
REL_NS = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"


def write_fixture(path):
    # 외부 Python 패키지나 실제 게임 workbook 없이 최소 OOXML fixture를 만든다.
    tables = [
        ("Enums", ["EnumId", "EnumName", "Target", "UnderlyingType", "Values"], ["uint32"] + ["string"] * 4,
         [[1, "ProbeKind", "Client", "uint8", "One=1|Two=2"]]),
        ("Alpha", ["Id", "Label", "Kind"], ["uint32", "string", "enum<ProbeKind>"], [[7, "한글", "Two"]]),
        ("std", ["Id", "Label", "Kind"], ["uint32", "string", "enum<ProbeKind>"], [[9, "shadow", "One"]]),
        ("Plain", ["Id", "Label", "SignedValue", "WideSigned", "WideUnsigned", "Ratio", "Precise", "Enabled", "Bonus"],
         ["uint32", "string", "int32", "int64", "uint64", "float", "double", "bool", "int32"],
         [[3, "plain", -7, -9223372036854775808, 18446744073709551615, 1.25, 2.5, "true", ""]]),
    ]
    workbook = ET.Element("workbook", {"xmlns": SHEET_NS, "xmlns:r": REL_NS})
    sheets = ET.SubElement(workbook, "sheets")
    relationships = ET.Element("Relationships", {"xmlns": "http://schemas.openxmlformats.org/package/2006/relationships"})
    types = ET.Element("Types", {"xmlns": "http://schemas.openxmlformats.org/package/2006/content-types"})
    ET.SubElement(types, "Default", Extension="rels", ContentType="application/vnd.openxmlformats-package.relationships+xml")
    ET.SubElement(types, "Default", Extension="xml", ContentType="application/xml")
    ET.SubElement(types, "Override", PartName="/xl/workbook.xml", ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml")
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
        for index, (name, fields, field_types, rows) in enumerate(tables, 1):
            ET.SubElement(sheets, "sheet", name=name, sheetId=str(index), attrib={"r:id": f"rId{index}"})
            ET.SubElement(relationships, "Relationship", Id=f"rId{index}", Type=REL_NS + "/worksheet", Target=f"worksheets/sheet{index}.xml")
            ET.SubElement(types, "Override", PartName=f"/xl/worksheets/sheet{index}.xml", ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml")
            values = {1: ["#Table", name], 2: ["#Target", "Shared" if name == "Enums" else "Client"]}
            labels = ["#Key", "#Scope", "#Type", "#Required", "#Min", "#Max", "#Default", "#Allowed", "#Reference", "#Field"]
            for row_number, label in enumerate(labels, 4):
                values[row_number] = [label]
            for column, field in enumerate(fields):
                values[4].append("Primary" if column == 0 else "")
                values[5].append("Shared" if name == "Enums" or column == 0 else "Client")
                values[6].append(field_types[column])
                values[7].append("false" if name == "Plain" and field == "Bonus" else "true")
                values[13].append(field)
            if name == "Plain":
                values[10] += [""] * (len(fields) - 1) + [17]
            for row_number, row in enumerate(rows, 14):
                values[row_number] = [""] + row
            worksheet = ET.Element("worksheet", {"xmlns": SHEET_NS})
            data = ET.SubElement(worksheet, "sheetData")
            for row_number, cells in sorted(values.items()):
                row = ET.SubElement(data, "row", r=str(row_number))
                for column, value in enumerate(cells):
                    if value == "":
                        continue
                    # 정확한 Excel 숫자 셀을 사용한다. 비키 64비트 경계 값의 텍스트 계약은 유지한다.
                    if isinstance(value, (int, float)) and abs(value) <= 9007199254740991:
                        cell = ET.SubElement(row, "c", r=f"{chr(65 + column)}{row_number}", t="n")
                        ET.SubElement(cell, "v").text = str(value)
                    else:
                        cell = ET.SubElement(row, "c", r=f"{chr(65 + column)}{row_number}", t="inlineStr")
                        ET.SubElement(ET.SubElement(cell, "is"), "t").text = str(value)
            archive.writestr(f"xl/worksheets/sheet{index}.xml", ET.tostring(worksheet, encoding="utf-8", xml_declaration=True))
        archive.writestr("xl/workbook.xml", ET.tostring(workbook, encoding="utf-8", xml_declaration=True))
        archive.writestr("xl/_rels/workbook.xml.rels", ET.tostring(relationships, encoding="utf-8", xml_declaration=True))
        archive.writestr("[Content_Types].xml", ET.tostring(types, encoding="utf-8", xml_declaration=True))
        archive.writestr("_rels/.rels", '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="' + REL_NS + '/officeDocument" Target="xl/workbook.xml"/></Relationships>')


def run(command, directory, label):
    result = subprocess.run(command, cwd=directory, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    log = directory / (label + ".log")
    log.write_bytes(result.stdout)
    commands_path = directory / "commands.json"
    commands = json.loads(commands_path.read_text(encoding="utf-8")) if commands_path.exists() else []
    commands.append({"name": label, "executed": True, "command": [str(part) for part in command],
                     "exit_code": result.returncode, "result_path": str(log),
                     "result_sha256": hashlib.sha256(result.stdout).hexdigest()})
    commands_path.write_text(json.dumps(commands, indent=2), encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"{label} failed ({result.returncode}); see {directory / (label + '.log')}")
    return result.stdout


def check_type_only_header(text, enum_header=False):
    if re.search(r'^\s*#\s*include\b', text, re.MULTILINE):
        raise AssertionError('generated header must be include-free')
    if re.search(r'(?<!:)\bstd::', text):
        raise AssertionError('shadowable standard type')
    if re.search(r'\bk\w+Rows\b|\bFind\w*\s*\(|::std::array<', text):
        raise AssertionError('generated header must contain types only')
    if enum_header:
        # 선언된 Enum 기호와 정확히 같은 기술 parser 블록만 인정한다. 다른 문자열은 계속 금지한다.
        definitions = {name: re.findall(r'^\s*(\w+)\s*=\s*-?\d+\s*,?\s*$', body, re.MULTILINE)
                       for name, body in re.findall(r'enum class E(\w+)\s*:\s*::std::\w+\s*\{([^}]+)\};', text)}
        for name, symbols in definitions.items():
            signature = f'\tinline bool TryParse{name}(const wchar_t* text, E{name}& output) noexcept\n'
            start = text.find(signature)
            if start < 0:
                raise AssertionError('missing enum parser')
            end = text.find('\n\t}', start) + len('\n\t}')
            body = text[start:end]
            blocks = re.findall(r'\t\t\{\n\t\t\tconst wchar_t\* expected = L"(\w+)";\n'
                                r'\t\t\tconst wchar_t\* actual = text;\n'
                                r'\t\t\twhile \(\*actual && \*actual == \*expected\)\n\t\t\t\{\n'
                                r'\t\t\t\t\+\+actual;\n\t\t\t\t\+\+expected;\n\t\t\t\}\n'
                                r'\t\t\tif \(\*actual == 0 && \*expected == 0\)\n\t\t\t\{\n'
                                rf'\t\t\t\toutput = E{re.escape(name)}::(\w+);\n'
                                r'\t\t\t\treturn true;\n\t\t\t\}\n\t\t\}', body)
            if blocks != [(symbol, symbol) for symbol in symbols] or body.count('L"') != len(symbols):
                raise AssertionError('enum parser symbols do not match declarations')
            # 다른 위치/추가 문자열은 소거하지 않는다.
            body = re.sub(r'(?m)^(\t\t\tconst wchar_t\* expected = )L"(' +
                          '|'.join(map(re.escape, symbols)) + r')";$', r'\1nullptr;', body)
            text = text[:start] + body + text[end:]
    if 'L"' in text:
        raise AssertionError('generated header must contain types only')


def check_header_guard_controls(text):
    check_type_only_header(text, True)
    mutations = [text + '\ninline const wchar_t* label = L"Gameplay display";\n',
                 text + '\ninline const wchar_t* label = L"Gold";\n',
                 text + '\n::std::array<int, 1> data{7};\n', text + '\nint kProbeRows;\n',
                 text + '\nvoid FindProbe();\n', text + '\n#include <string>\n',
                 text + '\nstd::wstring shadow;\n',
                 text.replace('const wchar_t* expected = L"', 'const wchar_t* expected = L"Undeclared', 1)]
    for mutation in mutations:
        try:
            check_type_only_header(mutation, True)
        except AssertionError:
            continue
        raise AssertionError('generated-header guard accepted injected gameplay/structural content')
    try:
        check_type_only_header('struct SRow { const wchar_t* label = L"Gold"; };')
    except AssertionError:
        return len(mutations) + 1
    raise AssertionError('ordinary table accepted a value literal')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--generator", required=True, type=Path, help="격리 빌드한 GameDataGenerator.dll")
    parser.add_argument("--scratch-root", required=True, type=Path)
    parser.add_argument("--vcvarsall", type=Path, help="생략하면 vswhere로 최신 MSVC를 찾는다")
    args = parser.parse_args()
    generator = args.generator.resolve(strict=True)
    args.scratch_root.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="generated-header-consumer-", dir=args.scratch_root.resolve()))
    print(f"logDirectory: {work}", flush=True)
    input_root = work / "Excel"
    input_root.mkdir()
    write_fixture(input_root / "Fixture.xlsx")
    cpp_root, yaml_root = work / "Generated/GameData", work / "Data"
    command = ["dotnet", str(generator), "--input-root", str(input_root), "--output-root", str(cpp_root), "--yaml-output-root", str(yaml_root)]
    run(command, work, "generate")
    run(command + ["--check"], work, "check")
    headers = sorted(cpp_root.rglob("*.g.h"))
    expected = {"Common/GameDataEnums.g.h", "Alpha/AlphaData.g.h", "std/stdData.g.h", "Plain/PlainData.g.h"}
    if {p.relative_to(cpp_root).as_posix() for p in headers} != expected:
        raise AssertionError("unexpected generated header set")
    for header in headers:
        check_type_only_header(header.read_text(encoding="utf-8-sig"),
                               header.relative_to(cpp_root).as_posix() == "Common/GameDataEnums.g.h")
    guard_controls = check_header_guard_controls((cpp_root / "Common/GameDataEnums.g.h").read_text(encoding="utf-8-sig"))
    for yaml in (yaml_root / "Client").glob("*.yaml"):
        if not yaml.read_bytes().startswith(b"# TextRPG.GameData 1\n"):
            raise AssertionError(f"missing exact YAML version marker: {yaml}")
    if (yaml_root / "Schema").exists() or list(yaml_root.rglob("*.schema.yaml")):
        raise AssertionError("schema output is forbidden")
    if '  "Bonus": 17\n' not in (yaml_root / "Client/Plain.yaml").read_text(encoding="utf-8"):
        raise AssertionError("optional field default was not preserved in YAML")
    (work / "EnumOnly.cpp").write_text('''#include <cstdint>
#include "Common/GameDataEnums.g.h"
static_assert(sizeof(::GameData::Common::EProbeKind) == sizeof(::std::uint8_t));
static_assert(static_cast<::std::uint8_t>(::GameData::Common::EProbeKind::Two) == 2);
''', encoding="utf-8")
    (work / "PlainOnly.cpp").write_text('''#include <array>
#include <cstdint>
#include <string>
#include "Plain/PlainData.g.h"
bool CheckPlain()
{
    ::GameData::Plain::SPlainData row{};
    if (row.id != 0 || !row.label.empty() || row.signedValue != 0 || row.wideSigned != 0 ||
        row.wideUnsigned != 0 || row.ratio != 0 || row.precise != 0 || row.enabled || row.bonus != 0) return false;
    row.id = 3;
    row.label = L"plain";
    row.signedValue = -7;
    row.wideSigned = (-9223372036854775807LL - 1LL);
    row.wideUnsigned = 18446744073709551615ULL;
    row.ratio = 1.25f;
    row.precise = 2.5;
    row.enabled = true;
    ::GameData::Plain::SPlainData::KeyType key = row.id;
    return key == 3 && row.label == L"plain" && row.signedValue == -7 && row.wideSigned < 0 &&
        row.wideUnsigned == 18446744073709551615ULL && row.ratio == 1.25f && row.precise == 2.5 && row.enabled;
}
''', encoding="utf-8")
    # std 테이블을 먼저 넣어 다음 데이터 헤더의 이름 가림도 실제로 검증한다.
    (work / "Consumer.cpp").write_text(r'''#include <array>
#include <cstdint>
#include <string>
#include "Common/GameDataEnums.g.h"
#include "std/stdData.g.h"
#include "Alpha/AlphaData.g.h"
bool CheckPlain();
int main()
{
    using ::GameData::Common::EProbeKind;
    ::GameData::std::SstdData shadow{};
    ::GameData::Alpha::SAlphaData alpha{};
    if (shadow.id != 0 || !shadow.label.empty() || !CheckPlain()) return 1;
    if (static_cast<::std::uint8_t>(alpha.kind) != 0) return 2;
    shadow = {9, L"shadow", EProbeKind::One};
    alpha = {7, L"\uD55C\uAE00", EProbeKind::Two};
    ::GameData::std::SstdData::KeyType key = shadow.id;
    if (key != 9 || shadow.label != L"shadow" || shadow.kind != EProbeKind::One) return 3;
    if (alpha.id != 7 || alpha.label != L"\uD55C\uAE00" || alpha.kind != EProbeKind::Two) return 4;
    if (alpha.label.size() != 2 || alpha.label[0] != 0xD55C || alpha.label[1] != 0xAE00) return 5;
    return 0;
}
''', encoding="utf-8")
    # 첫 include부터 표준 전제/PCH 없이 umbrella만 읽고 중복 포함 안전성도 검사한다.
    (work / "UmbrellaOnly.cpp").write_text(r'''#include "GameDataAll.h"
#include "GameDataAll.h"
static_assert(sizeof(::GameData::Common::EProbeKind) == sizeof(::std::uint8_t));
int main()
{
    ::GameData::Alpha::SAlphaData alpha{};
    ::GameData::Plain::SPlainData plain{};
    ::GameData::std::SstdData shadow{};
    if (alpha.id != 0 || !alpha.label.empty() || plain.bonus != 0 || shadow.id != 0) return 1;
    alpha = {7, L"\uD55C\uAE00", ::GameData::Common::EProbeKind::Two};
    plain.label = L"plain";
    shadow = {9, L"shadow", ::GameData::Common::EProbeKind::One};
    ::std::array<::GameData::Plain::SPlainData::KeyType, 1> keys{plain.id};
    if (keys[0] != 0 || plain.label != L"plain" || shadow.label != L"shadow") return 2;
    if (alpha.label.size() != 2 || alpha.label[0] != 0xD55C || alpha.label[1] != 0xAE00) return 3;
    return alpha.kind == ::GameData::Common::EProbeKind::Two ? 0 : 4;
}
''', encoding="utf-8")
    vcvars = args.vcvarsall
    if vcvars is None:
        vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
        installation = run([str(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], work, "vswhere").decode("utf-8-sig").strip()
        if not installation:
            raise RuntimeError("MSVC installation not found")
        vcvars = Path(installation) / "VC/Auxiliary/Build/vcvarsall.bat"
    vcvars = vcvars.resolve(strict=True)
    batch = work / "compile.cmd"
    batch.write_text(f'@echo off\ncall "{vcvars}" x64\nif errorlevel 1 exit /b %errorlevel%\ncl /nologo /std:c++20 /permissive- /utf-8 /EHsc /Y- /W4 /WX /analyze /I Generated/GameData Consumer.cpp EnumOnly.cpp PlainOnly.cpp /Fe:Consumer.exe\nif errorlevel 1 exit /b %errorlevel%\ncl /nologo /std:c++20 /permissive- /utf-8 /EHsc /Y- /W4 /WX /analyze /I Generated/GameData UmbrellaOnly.cpp /Fe:UmbrellaOnly.exe\nexit /b %errorlevel%\n', encoding="utf-8")
    run([os.environ.get("COMSPEC", "cmd.exe"), "/d", "/c", str(batch)], work, "compile")
    run([str(work / "Consumer.exe")], work, "consumer")
    run([str(work / "UmbrellaOnly.exe")], work, "umbrella-consumer")
    umbrella = (cpp_root / "GameDataAll.h").read_text(encoding="utf-8-sig")
    expected_includes = ['<array>', '<cstdint>', '<string>', '"Common/GameDataEnums.g.h"',
                         '"Alpha/AlphaData.g.h"', '"Plain/PlainData.g.h"', '"std/stdData.g.h"']
    expected_umbrella = '#pragma once\n\n// Generated by GameDataGenerator. Do not edit.\n' + ''.join(
        f'#include {include}\n' for include in expected_includes)
    if umbrella != expected_umbrella:
        raise AssertionError("umbrella must contain only prerequisites and enum-first/ordinal emitted includes")
    if {p.relative_to(cpp_root).as_posix() for p in cpp_root.rglob("*.h")} != expected | {"GameDataAll.h"}:
        raise AssertionError("unexpected umbrella, runtime header, or secondary alias")
    result = {"passed": True, "guardNegativeControls": guard_controls,
              "headers": len(headers) + 1, "leafHeaders": len(headers), "translationUnits": 4,
              "compileExitCode": 0, "consumerExitCode": 0, "umbrellaConsumerExitCode": 0, "logDirectory": str(work)}
    (work / "results.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
