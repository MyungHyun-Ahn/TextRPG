"""실제 생성기 CLI의 Scene/UI 계약 및 실패 시 출력 보존 통합 검사.

openpyxl이 필요하다. --generator는 빌드된 GameDataGenerator.dll,
--scratch-root는 원본 밖의 소유한 임시 작업 디렉터리로 지정한다.
원본 workbook/asset/generated 파일은 읽기만 한다.
"""
import argparse
import codecs
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import uuid

from openpyxl import load_workbook
from test_scene_ui_data import ROOT, ASSETS, table_records


def column(sheet, field):
    return next(c for c in range(2, sheet.max_column + 1) if sheet.cell(13, c).value == field)


def set_value(workbook, sheet, field, row, value):
    workbook[sheet].cell(row, column(workbook[sheet], field)).value = value


def change_config(workbook, field, value):
    sheet = workbook['SceneTypeConfig']
    col = column(sheet, field)
    sheet.cell(8, col).value = None
    sheet.cell(9, col).value = None
    sheet.cell(11, col).value = None
    sheet.cell(14, col).value = value


def fifth_option(workbook, _):
    sheet = workbook['SceneOption']
    sheet.cell(9, column(sheet, 'OptionOrder')).value = 8
    sheet.append([None, 999, 'MainMenu', 5, 4, 'Credits', 0, False])


def no_battle_options(workbook, _):
    sheet = workbook['SceneOption']
    for row in range(sheet.max_row, 13, -1):
        if sheet.cell(row, column(sheet, 'SceneType')).value == 'BattleField':
            sheet.delete_rows(row)


def add_forbidden_scene_binding(workbook, name, scope='Shared'):
    sheet = workbook['Scene']
    col = sheet.max_column + 1
    sheet.cell(5, col, scope)
    sheet.cell(6, col, 'string')
    sheet.cell(7, col, 'true')
    sheet.cell(13, col, name)
    for row in range(14, sheet.max_row + 1):
        sheet.cell(row, col, 'runtime binding must not be here')


def edit_art(transform):
    def mutate(_, data_root):
        path = data_root / ASSETS[1][1]
        path.write_bytes(transform(path.read_bytes()))
    return mutate


def generic_key(workbook, kind, value):
    sheet = workbook.copy_worksheet(workbook['Localization'])
    sheet.title = 'FutureSkill'
    sheet['B1'] = 'FutureSkill'
    sheet.delete_rows(15, sheet.max_row - 14)
    sheet['B13'] = 'SkillId'
    sheet['B6'] = kind
    sheet['B14'] = value


def numeric_cases():
    for kind in ('string', 'bool', 'float', 'double', 'enum<SceneType>'):
        yield ('numeric_pk_type_' + kind.replace('<', '_').replace('>', '_'), 'FutureSkill',
               lambda wb, _, kind=kind: generic_key(wb, kind, 1))
    for name, value in [('text_integer', '001'), ('legacy_name', 'Fireball'), ('bool', True),
                        ('fraction', 1.5), ('zero', 0), ('negative', -1), ('overflow', 4294967296),
                        ('nan_text', 'NaN')]:
        yield ('numeric_pk_' + name, 'FutureSkill', lambda wb, _, value=value: generic_key(wb, 'uint32', value))
    yield ('numeric_target_text', 'SceneOption', lambda wb, _: set_value(wb, 'SceneOption', 'TargetSceneId', 14, '002'))
    yield ('numeric_target_bool', 'SceneOption', lambda wb, _: set_value(wb, 'SceneOption', 'TargetSceneId', 14, True))
    yield ('numeric_target_fraction', 'SceneOption', lambda wb, _: set_value(wb, 'SceneOption', 'TargetSceneId', 14, 2.5))
    yield ('numeric_target_default_missing', 'SceneOption', lambda wb, _: set_value(wb, 'SceneOption', 'TargetSceneId', 10, None))
    yield ('numeric_duplicate_config_type', 'SceneTypeConfig', lambda wb, _: set_value(wb, 'SceneTypeConfig', 'SceneType', 15, 'MainMenu'))
    yield ('numeric_enum_name_duplicate', 'Enums', lambda wb, _: set_value(wb, 'Enums', 'EnumName', 15, 'SceneType'))


def cases():
    return [
        ('enum_allowed_redefinition', 'Scene', lambda wb, _: set_value(wb, 'Scene', 'SceneType', 11, 'MainMenu=0|Town=1|BattleField=2')),
        ('undefined_enum', 'Scene', lambda wb, _: set_value(wb, 'Scene', 'SceneType', 6, 'enum<MissingType>')),
        ('invalid_enum_value', 'Scene', lambda wb, _: set_value(wb, 'Scene', 'SceneType', 14, 'MissingValue')),
        ('missing_enum_definitions', 'Enums', lambda wb, _: wb.remove(wb['Enums'])),
        ('ignored_scene_type', 'Scene', lambda wb, _: set_value(wb, 'Scene', 'SceneType', 5, 'Ignore')),
        ('forbidden_player_binding', 'Scene', lambda wb, _: add_forbidden_scene_binding(wb, 'PlayerArtId')),
        ('unexpected_scene_shared_field', 'Scene', lambda wb, _: add_forbidden_scene_binding(wb, 'CurrentMonsterName')),
        ('unexpected_scene_client_field', 'Scene', lambda wb, _: add_forbidden_scene_binding(wb, 'CurrentMonsterName', 'Client')),
        ('unexpected_scene_ignored_field', 'Scene', lambda wb, _: add_forbidden_scene_binding(wb, 'CurrentMonsterName', 'Ignore')),
        ('unexpected_scene_shared_legacy_asset', 'Scene', lambda wb, _: add_forbidden_scene_binding(wb, 'AssetPath')),
        ('unexpected_scene_client_legacy_asset', 'Scene', lambda wb, _: add_forbidden_scene_binding(wb, 'AssetPath', 'Client')),
        ('unexpected_scene_ignored_legacy_asset', 'Scene', lambda wb, _: add_forbidden_scene_binding(wb, 'AssetPath', 'Ignore')),
        ('missing_battle_config', 'Scene', lambda wb, _: wb['SceneTypeConfig'].delete_rows(16)),
        ('removed_target_reference', 'SceneOption', lambda wb, _: set_value(wb, 'SceneOption', 'TargetSceneId', 12, None)),
        ('removed_text_reference', 'SceneOption', lambda wb, _: set_value(wb, 'SceneOption', 'OptionTextKey', 12, None)),
        ('forbidden_content_binding', 'Scene', lambda wb, _: add_forbidden_scene_binding(wb, 'ContentArtId')),
        ('forbidden_title_binding', 'Scene', lambda wb, _: add_forbidden_scene_binding(wb, 'TitleTextKey')),
        ('missing_active_target', 'SceneOption', lambda wb, _: set_value(wb, 'SceneOption', 'TargetSceneId', 14, None)),
        ('unknown_active_target', 'SceneOption', lambda wb, _: set_value(wb, 'SceneOption', 'TargetSceneId', 14, 999)),
        ('unknown_localization_key', 'SceneOption', lambda wb, _: set_value(wb, 'SceneOption', 'OptionTextKey', 14, 999)),
        ('legacy_string_target', 'SceneOption', lambda wb, _: set_value(wb, 'SceneOption', 'TargetSceneId', 14, 'MissingScene')),
        ('legacy_string_text_key', 'SceneOption', lambda wb, _: set_value(wb, 'SceneOption', 'OptionTextKey', 14, 'Missing.String.Key')),
        ('disabled_unknown_target', 'SceneOption', lambda wb, _: set_value(wb, 'SceneOption', 'TargetSceneId', 15, 999)),
        ('ignored_required_localization', 'Localization', lambda wb, _: set_value(wb, 'Localization', 'KoKR', 5, 'Ignore')),
        ('three_options', 'SceneTypeConfig', lambda wb, _: wb['SceneOption'].delete_rows(15)),
        ('five_options', 'SceneTypeConfig', fifth_option),
        ('zero_options', 'SceneTypeConfig', no_battle_options),
        ('configured_five', 'SceneTypeConfig', lambda wb, _: change_config(wb, 'MaxVisibleOptions', 5)),
        ('scroll_enabled', 'SceneTypeConfig', lambda wb, _: change_config(wb, 'OptionOverflowPolicy', 'Scroll')),
        ('forbidden_region_binding', 'Scene', lambda wb, _: add_forbidden_scene_binding(wb, 'RegionTextKey')),
        ('missing_static_scene_type', 'Scene', lambda wb, _: wb['Scene'].delete_cols(column(wb['Scene'], 'SceneType'))),
        ('wrong_target_reference_type', 'SceneOption', lambda wb, _: set_value(wb, 'SceneOption', 'TargetSceneId', 6, 'string')),
        ('missing_art_file', 'TextArt', lambda wb, _: set_value(wb, 'TextArt', 'AssetPath', 15, 'Asset/Character/missing.txt')),
        ('rooted_art_path', 'TextArt', lambda wb, _: set_value(wb, 'TextArt', 'AssetPath', 15, '/Asset/outside.txt')),
        ('escaping_art_path', 'TextArt', lambda wb, _: set_value(wb, 'TextArt', 'AssetPath', 15, '../outside.txt')),
        ('alternate_stream_path', 'TextArt', lambda wb, _: set_value(wb, 'TextArt', 'AssetPath', 15, ASSETS[1][1] + ':stream')),
        ('invalid_utf8', 'TextArt', edit_art(lambda data: b'\xff' + data[1:])),
        ('utf16_not_utf8', 'TextArt', edit_art(lambda data: data.decode('utf-8').encode('utf-16'))),
        ('tab_in_art', 'TextArt', edit_art(lambda data: b'\t' + data[1:])),
        ('extra_blank_row', 'TextArt', edit_art(lambda data: data + b'\n')),
        ('trailing_spaces_removed', 'TextArt', edit_art(lambda data: ('\n'.join(row.rstrip() for row in data.decode('utf-8').splitlines()) + '\n').encode('utf-8'))),
        ('combining_character', 'TextArt', edit_art(lambda data: '\u0301'.encode('utf-8') + data[1:])),
        ('cjk_in_art', 'TextArt', edit_art(lambda data: '한'.encode('utf-8') + data[1:])),
        ('emoji_in_art', 'TextArt', edit_art(lambda data: '😀'.encode('utf-8') + data[1:])),
    ]


def snapshot(directory):
    result = {}
    for path in directory.rglob('*'):
        relative = path.relative_to(directory).as_posix()
        result[relative] = 'directory' if path.is_dir() else hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generator', required=True, type=Path)
    parser.add_argument('--scratch-root', required=True, type=Path)
    parser.add_argument('--workbook', type=Path, help='검사할 workbook 사본. 기본값은 프로젝트 원본')
    options = parser.parse_args()
    generator = options.generator.resolve(strict=True)
    scratch = options.scratch_root.resolve() / ('scene-contract-' + uuid.uuid4().hex)
    scratch.mkdir(parents=True, exist_ok=False)
    source_workbook = (options.workbook or ROOT / 'GameData/Excel/Scene.xlsx').resolve(strict=True)
    source_hash = hashlib.sha256(source_workbook.read_bytes()).hexdigest()
    outputs = [scratch / 'output' / 'Generated/GameData', scratch / 'output' / 'Data']
    env = os.environ.copy()
    for name in ('TMP', 'TEMP', 'TMPDIR'):
        env[name] = str(scratch)
    env['LIB'] = ''
    results = []

    def persist():
        (scratch / 'results.json').write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding='utf-8')

    def fixture(name):
        root = scratch / 'cases' / name / 'GameData'
        (root / 'Excel').mkdir(parents=True)
        shutil.copy2(source_workbook, root / 'Excel/Scene.xlsx')
        shutil.copytree(source_workbook.parent.parent / 'Asset', root / 'Asset')
        assert not (root / 'Asset/Scene').exists(), 'legacy Scene assets must be absent'
        return root

    def run(name, data_root, validate_only=False):
        args = ['dotnet', str(generator)] if generator.suffix.lower() == '.dll' else [str(generator)]
        args += ['--input-root', str(data_root / 'Excel'), '--output-root', str(outputs[0]), '--yaml-output-root', str(outputs[1])]
        if validate_only:
            args.append('--validate-only')
        completed = subprocess.run(args, cwd=ROOT, env=env, capture_output=True, encoding='utf-8', errors='replace', timeout=60)
        text = completed.stdout + completed.stderr
        (scratch / (name + '.log')).write_text(text, encoding='utf-8')
        return completed.returncode, text

    data_root = fixture('valid')
    code, text = run('valid', data_root)
    results.append({'case': 'valid', 'exit': code, 'passed': code == 0})
    persist()
    if code != 0:
        raise AssertionError(text)
    assert (outputs[0] / 'Common/GameDataEnums.g.h').is_file(), 'enum C++ header missing'
    assert (outputs[1] / 'Client/Scene.yaml').is_file(), 'client YAML missing'
    scene_header = (outputs[0] / 'Scene/SceneData.g.h').read_text(encoding='utf-8-sig')
    scene_yaml = (outputs[1] / 'Client/Scene.yaml').read_text(encoding='utf-8')
    assert 'assetPath' not in scene_header and '"AssetPath"' not in scene_yaml, 'legacy Scene field must not be generated'
    assert 'assetPath' in (outputs[0] / 'TextArt/TextArtData.g.h').read_text(encoding='utf-8-sig')
    assert '"AssetPath"' in (outputs[1] / 'Client/TextArt.yaml').read_text(encoding='utf-8')
    for header in outputs[0].rglob('*.g.h'):
        text = header.read_text(encoding='utf-8-sig')
        assert not re.search(r'\bk\w+Rows\b|\bFind\w*\s*\(|::std::array<|L"', text), f'not types-only: {header}'
        assert not re.search(r'^\s*#\s*include\b', text, re.MULTILINE), f'not include-free: {header}'
    for yaml in (outputs[1] / 'Client').glob('*.yaml'):
        assert yaml.read_bytes().startswith(b'# TextRPG.GameData 1\n'), f'missing exact version marker: {yaml}'
    option_yaml = (outputs[1] / 'Client/SceneOption.yaml').read_text(encoding='utf-8')
    assert '  "TargetSceneId": 0\n' in option_yaml, 'absent optional reference must emit numeric zero in YAML'
    assert '  "TargetSceneId": null\n' not in option_yaml
    assert not any(path.name == 'Schema' or path.name.endswith('.schema.yaml')
                   for root in outputs for path in root.rglob('*')), 'schema output must be absent'
    baseline = [snapshot(path) for path in outputs]
    for name, expected_sheet, mutate in cases() + list(numeric_cases()):
        data_root = fixture(name)
        path = data_root / 'Excel/Scene.xlsx'
        wb = load_workbook(path)
        mutate(wb, data_root)
        wb.save(path)
        wb.close()
        input_before = hashlib.sha256(path.read_bytes()).hexdigest()
        code, text = run(name, data_root)
        unchanged = baseline == [snapshot(path) for path in outputs]
        preserved = input_before == hashlib.sha256(path.read_bytes()).hexdigest()
        located = 'Scene.xlsx' in text and re.search(r'\[[^\]]+\]\s+[A-Z]+\d+', text) is not None
        expected_code = ('GD2132' if name.startswith('forbidden_') else
                         'GD2133' if name.startswith('unexpected_scene_') else None)
        if name.startswith('numeric_pk_type_'):
            expected_code = 'GD0307'
        elif name.startswith('numeric_'):
            expected_code = {'numeric_pk_zero': 'GD0331', 'numeric_pk_negative': 'GD0202',
                             'numeric_pk_overflow': 'GD0202', 'numeric_target_default_missing': 'GD0324',
                             'numeric_duplicate_config_type': 'GD2116', 'numeric_enum_name_duplicate': 'GD0903'}.get(name, 'GD0210')
        enum_code = {'enum_allowed_redefinition': 'GD0909', 'undefined_enum': 'GD0908',
                     'invalid_enum_value': 'GD0911', 'missing_enum_definitions': 'GD0901'}.get(name)
        if enum_code is not None:
            located = located and f'error {enum_code}:' in text
        if expected_code in ('GD2132', 'GD2133'):
            located = any(line.startswith(f'{path} [Scene] E13: error {expected_code}:')
                          for line in text.splitlines())
        elif expected_code is not None:
            located = any(re.match(re.escape(f'{path} [{expected_sheet}] ') +
                                  rf'[A-Z]+\d+: error {expected_code}:', line) for line in text.splitlines())
        passed = code != 0 and unchanged and preserved and located
        results.append({'case': name, 'exit': code, 'passed': passed, 'outputPreserved': unchanged,
                        'inputPreserved': preserved, 'locatedDiagnostic': located, 'expectedDomain': expected_sheet,
                        'expectedCode': expected_code, 'expectedCell': 'E13' if expected_code in ('GD2132', 'GD2133') else None})
        persist()
        if not passed:
            raise AssertionError(f'{name}: invalid workbook accepted, output/input changed or unlocated diagnostic\n{text}')

    for name, transform in [('utf8_bom', lambda data: codecs.BOM_UTF8 + data),
                            ('crlf', lambda data: data.replace(b'\n', b'\r\n'))]:
        data_root = fixture(name)
        for _, path, _, _ in ASSETS:
            asset = data_root / path
            asset.write_bytes(transform(asset.read_bytes()))
        code, text = run(name, data_root, validate_only=True)
        results.append({'case': name, 'exit': code, 'passed': code == 0})
        persist()
        if code != 0:
            raise AssertionError(text)

    data_root = fixture('nested_workbook')
    nested = data_root / 'Excel/Sub/Scene.xlsx'
    nested.parent.mkdir()
    (data_root / 'Excel/Scene.xlsx').rename(nested)
    code, text = run('nested_workbook', data_root, validate_only=True)
    results.append({'case': 'nested_workbook', 'exit': code, 'passed': code == 0})
    persist()
    if code != 0:
        raise AssertionError(text)
    # 현재 표시 대상은 Scene 데이터가 아닌 런타임 구성 객체가 결정한다.
    # 따라서 독립 카탈로그가 없거나 ID/현지화 엔트리가 늘어도 Scene 행은 바꾸지 않는다.
    for name in ['scene_without_art_catalog', 'independent_catalog_ids', 'additional_entity_name_key', 'generic_extra_field_allowed']:
        data_root = fixture(name)
        path = data_root / 'Excel/Scene.xlsx'
        wb = load_workbook(path)
        original_scenes = table_records(wb['Scene'])
        if name == 'scene_without_art_catalog':
            wb.remove(wb['TextArt'])
        elif name == 'independent_catalog_ids':
            sheet = wb['TextArt']
            for row in range(14, sheet.max_row + 1):
                sheet.cell(row, column(sheet, 'ArtId')).value += 100
        elif name == 'additional_entity_name_key':
            wb['Localization'].append([None, 1001, '다른 몬스터', 'Other Monster'])
        else:
            sheet = wb.copy_worksheet(wb['Localization'])
            sheet.title = 'Independent'
            sheet['B1'] = 'Independent'
            sheet['C13'] = 'CurrentMonsterName'
        assert original_scenes == table_records(wb['Scene'])
        wb.save(path)
        wb.close()
        code, text = run(name, data_root, validate_only=True)
        results.append({'case': name, 'exit': code, 'passed': code == 0})
        persist()
        if code != 0:
            raise AssertionError(text)
    if source_hash != hashlib.sha256(source_workbook.read_bytes()).hexdigest():
        raise AssertionError('original workbook was modified')
    print(json.dumps({'result': 'PASS', 'cases': len(results), 'logDirectory': str(scratch)}, ensure_ascii=False))


if __name__ == '__main__':
    main()
