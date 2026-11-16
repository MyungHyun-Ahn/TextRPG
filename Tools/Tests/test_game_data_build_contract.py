"""MSBuild GameData.dat 배포 계약 검사. 실제 패킹/실행 검증은 별도로 수행한다."""
from pathlib import Path
import json
import subprocess
import tempfile
import unittest
from xml.sax.saxutils import escape
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
PACKER_GUID = "{E3145A6F-EC7F-46A7-9F63-CC75CC4FE405}"


def registered_items(project, kind):
    # Target 안의 remove/reload는 실행 시 항목이다. IDE 등록 중복과 별도로 검증한다.
    return [item for item in project.findall('m:ItemGroup/m:' + kind, NS) if item.get('Include')]


def expanded_paths(base, items):
    paths = []
    for item in items:
        name = item.get('Include')
        if '*' in name:
            matches = sorted(base.glob(name.replace('\\', '/')))
            if not matches:
                raise AssertionError('registered generated glob has no published files: ' + name)
            paths.extend(str(path.relative_to(base)) for path in matches)
        else:
            paths.append(name)
    return paths


class GameDataBuildContract(unittest.TestCase):
    def test_ingame_reference_evaluation_keeps_dependency_output_roots(self):
        from native_build import _visual_studio, _property
        with tempfile.TemporaryDirectory(prefix='ingame-reference-paths-') as directory:
            folder = Path(directory)
            for relative in ('NativeTests.vcxproj', 'Tools/GameDataPacker/GameDataPacker.vcxproj'):
                command = [str(_visual_studio()), str(ROOT / relative), '-nologo',
                           '-t:ResolveProjectReferences', '-p:BuildProjectReferences=false',
                           '-p:Configuration=Debug', '-p:Platform=x64', '-p:NativeTestSuite=InGameData',
                           '-p:NativeUseGameDataLibrary=true',
                           '-p:OutDir=' + _property(str(folder / 'consumer-output') + '/'),
                           '-p:IntDir=' + _property(str(folder / 'consumer-obj') + '/'),
                           '-getItem:_ResolvedNativeProjectReferencePaths']
                executed = subprocess.run(command, capture_output=True, timeout=30)
                self.assertEqual(executed.returncode, 0, executed.stdout + executed.stderr)
                items = json.loads(executed.stdout.decode('utf-8-sig'))['Items']['_ResolvedNativeProjectReferencePaths']
                paths = {Path(item['FullPath']).resolve() for item in items}
                self.assertTrue({(ROOT / 'Out' / name / 'Debug/x64' / (name + '.lib')).resolve()
                                 for name in ('ConsoleGameCore', 'GameData')} <= paths)
                self.assertFalse(any(path.is_relative_to(folder) for path in paths))
                print(json.dumps({'scope': 'read-only-MSBuild-reference-evaluation', 'command': command,
                                  'resolvedNativeLibraries': sorted(str(path) for path in paths)}))

    def test_ingame_dynamic_refresh_executes_after_evaluation(self):
        from native_build import _visual_studio
        with tempfile.TemporaryDirectory(prefix='ingame-msbuild-items-') as directory:
            folder = Path(directory)
            stale = folder / 'Stale.g.cpp'
            fresh = folder / 'Fresh.g.cpp'
            stale.write_text('// removed fixture type\n', encoding='utf-8')
            extension = folder / 'fixture.targets'
            extension.write_text('''<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup><ClCompile Include="''' + escape(str(stale)) + '''"><InGameDataGenerated>true</InGameDataGenerated></ClCompile></ItemGroup>
  <Target Name="MutateGeneratedItemsFixture" BeforeTargets="RefreshInGameDataCompileItems">
    <Delete Files="''' + escape(str(stale)) + '''" />
    <WriteLinesToFile File="''' + escape(str(fresh)) + '''" Lines="// new fixture type" Overwrite="true" />
  </Target>
</Project>''', encoding='utf-8')
            command = [str(_visual_studio()), str(ROOT / 'GameData/GameData.vcxproj'), '-nologo',
                       '-t:RefreshInGameDataCompileItems', '-getItem:ClCompile', '-p:DesignTimeBuild=true',
                       '-p:Configuration=Debug', '-p:Platform=x64', '-p:InGameDataGeneratedRoot=' + str(folder),
                       '-p:ForceImportAfterCppTargets=' + str(extension)]
            executed = subprocess.run(command, capture_output=True, timeout=30)
            self.assertEqual(executed.returncode, 0, executed.stdout + executed.stderr)
            items = json.loads(executed.stdout.decode('utf-8-sig'))['Items']['ClCompile']
            generated = [item for item in items if item.get('InGameDataGenerated') == 'true']
            self.assertEqual([Path(item['FullPath']) for item in generated], [fresh])
            self.assertFalse(stale.exists())
            self.assertEqual(generated[0]['PrecompiledHeader'], 'Use')
            self.assertEqual(generated[0]['PrecompiledHeaderFile'], 'GameData/pch.h')
            self.assertEqual(generated[0]['TreatWarningAsError'], 'true')
            print(json.dumps({'scope': 'real-MSBuild-item-refresh-no-native-compilation',
                              'command': command, 'generatedItems': generated}, ensure_ascii=False))

    def test_ingame_fixture_objects_use_each_generated_source_identity(self):
        from native_build import _visual_studio
        with tempfile.TemporaryDirectory(prefix='ingame-fixture-objects-') as directory:
            command = [str(_visual_studio()), str(ROOT / 'NativeTests.vcxproj'), '-nologo',
                       '-t:RefreshInGameDataFixtureCompileItems', '-getItem:ClCompile',
                       '-p:DesignTimeBuild=true', '-p:NativeTestSuite=InGameData',
                       '-p:NativeUseGameDataLibrary=true', '-p:Configuration=Debug', '-p:Platform=x64',
                       '-p:IntDir=' + str(Path(directory)) + '/']
            executed = subprocess.run(command, capture_output=True, timeout=30)
            self.assertEqual(executed.returncode, 0, executed.stdout + executed.stderr)
            items = json.loads(executed.stdout.decode('utf-8-sig'))['Items']['ClCompile']
            fixtures = [item for item in items if item.get('InGameDataFixture') == 'true']
            expected_sources = sorted((ROOT / 'Out/Tests/InGameDataSchema').glob('*/*.g.cpp'))
            self.assertGreater(len(expected_sources), 1)
            self.assertEqual(sorted(Path(item['FullPath']) for item in fixtures), expected_sources)
            objects = []
            for item in fixtures:
                source = Path(item['FullPath'])
                expected = Path(directory) / ('Schema' + source.parent.name + '_' + source.stem + '.obj')
                self.assertEqual(Path(item['ObjectFileName']), expected)
                self.assertEqual(item['PrecompiledHeaderFile'], 'GameData/pch.h')
                objects.append(item['ObjectFileName'].casefold())
            self.assertEqual(len(objects), len(set(objects)))
            print(json.dumps({'scope': 'real-MSBuild-fixture-object-identity-no-compilation',
                              'generatedSources': len(fixtures), 'distinctObjects': len(set(objects))}))

    def test_generated_fixture_targets_are_ingame_only_and_cache_preserving(self):
        native = ET.parse(ROOT / 'NativeTests.vcxproj').getroot()
        target = native.find("m:Target[@Name='GenerateInGameDataFixtureBeforeCompile']", NS)
        self.assertEqual(target.get('Condition'), "'$(DesignTimeBuild)'!='true' and '$(NativeTestSuite)'=='InGameData'")
        executions = target.findall('m:Exec', NS)
        self.assertEqual(len(executions), 4)
        for name, namespace, pair in (('CompatibilityV1', 'TextRPGTest::InGameDataV1', executions[:2]),
                                      ('SharedGraph', 'TextRPGTest::InGameData', executions[2:])):
            for execution in pair:
                command = execution.get('Command', '')
                self.assertIn('Tests\\InGameData\\Schema\\' + name + '.yaml', command)
                self.assertIn('Out\\Tests\\InGameDataSchema\\' + name, command)
                self.assertIn('--ingame-namespace ' + namespace, command)
                self.assertIn('LIB=', execution.get('EnvironmentVariables', ''))
            self.assertIn('--check', pair[0].get('Command'))
            self.assertEqual(pair[0].get('IgnoreExitCode'), 'true')
            self.assertNotEqual(pair[1].get('IgnoreExitCode'), 'true')
            property_name = pair[0].find('m:Output', NS).get('PropertyName')
            self.assertIn(property_name, pair[1].get('Condition'))
        refresh = native.find("m:Target[@Name='RefreshInGameDataFixtureCompileItems']", NS)
        self.assertEqual(refresh.get('DependsOnTargets'), target.get('Name'))
        self.assertEqual(refresh.get('Condition'), "'$(NativeTestSuite)'=='InGameData'")
        self.assertEqual(set(refresh.get('BeforeTargets').split(';')), {'BeforeClCompile', 'SelectClCompile'})
        entries = refresh.findall('m:ItemGroup/m:ClCompile', NS)
        self.assertTrue(any(item.get('Remove') == '@(ClCompile)' and 'InGameDataFixture' in item.get('Condition', '') for item in entries))
        added = [item for item in entries if item.get('Include')]
        self.assertEqual({item.get('Include') for item in added}, {'@(_NativeSchemaSharedGraph)',
                                                                 '@(_NativeSchemaCompatibilityV1)'})
        for item in added:
            self.assertEqual(item.findtext('m:PrecompiledHeaderFile', namespaces=NS), 'GameData/pch.h')
            self.assertEqual(item.findtext('m:PrecompiledHeader', namespaces=NS), 'Use')
            source_items = item.get('Include')[2:-1]
            cohort = source_items.removeprefix('_NativeSchema')
            self.assertEqual(refresh.find('m:ItemGroup/m:' + source_items, NS).get('Include'),
                             'Out\\Tests\\InGameDataSchema\\' + cohort + '\\*.g.cpp')
            self.assertEqual(item.findtext('m:ObjectFileName', namespaces=NS),
                             '$(IntDir)Schema' + cohort + '_%(' + source_items + '.Filename).obj')
        self.assertFalse(any('Out\\' in item.get('Include', '') for item in registered_items(native, 'ClCompile')))

    def test_ingame_generation_refreshes_compile_inputs_before_cpp_selection(self):
        data = ET.parse(ROOT / 'GameData/GameData.vcxproj').getroot()
        generation = data.find("m:Target[@Name='GenerateInGameDataBeforeCompile']", NS)
        self.assertIsNotNone(generation, 'YAML generation must precede refreshed compile input discovery')
        commands = generation.findall('m:Exec', NS)
        self.assertEqual(len(commands), 2)
        self.assertIn('--check', commands[0].get('Command', ''))
        self.assertEqual(commands[0].get('IgnoreExitCode'), 'true')
        self.assertNotEqual(commands[1].get('IgnoreExitCode'), 'true')
        self.assertIn('InGameDataGenerationCheckExitCode', commands[1].get('Condition', ''))
        for execution in commands:
            self.assertIn('--ingame-schema', execution.get('Command', ''))
            self.assertIn('--ingame-output-root', execution.get('Command', ''))
            self.assertIn('LIB=', execution.get('EnvironmentVariables', ''))
        refresh = data.find("m:Target[@Name='RefreshInGameDataCompileItems']", NS)
        self.assertIsNotNone(refresh)
        self.assertIn('GenerateInGameDataBeforeCompile', refresh.get('DependsOnTargets', ''))
        self.assertTrue({'BeforeClCompile', 'SelectClCompile'} <= set(refresh.get('BeforeTargets', '').split(';')))
        items = refresh.findall('m:ItemGroup/m:ClCompile', NS)
        self.assertTrue(any(item.get('Remove') == '@(ClCompile)' and 'InGameDataGenerated' in item.get('Condition', '') for item in items))
        added = [item for item in items if item.get('Include')]
        self.assertEqual(len(added), 1)
        self.assertIn('*.g.cpp', added[0].get('Include'))
        self.assertEqual(added[0].findtext('m:InGameDataGenerated', namespaces=NS), 'true')
        game = ET.parse(ROOT / 'TextRPG/TextRPG.vcxproj').getroot()
        self.assertFalse(any('.g.cpp' in item.get('Include', '') for item in game.findall('.//m:ClCompile', NS)))
        self.assertEqual([item.get('Include') for item in registered_items(data, 'None')
                          if item.get('Include', '').startswith('Generated\\InGameData\\')],
                         ['Generated\\InGameData\\.ingamedata-output'])
        for relative in ('GameData/GameData.vcxproj', 'TextRPG/TextRPG.vcxproj', 'NativeTests.vcxproj'):
            self.assertNotIn('GameData\\Class', (ROOT / relative).read_text(encoding='utf-8-sig'))
        native = ET.parse(ROOT / 'NativeTests.vcxproj').getroot()
        variant = next(group for group in native.findall('m:ItemGroup', NS)
                       if any(item.get('Include') == 'GameData\\Generated\\InGameData\\*.g.cpp' for item in group))
        self.assertEqual(variant.get('Condition'), "('$(NativeTestSuite)'=='SceneUiRuntime' or '$(NativeTestSuite)'=='InGameData') and '$(NativeUseGameDataLibrary)'!='true'")
        refresh = native.find("m:Target[@Name='RefreshNativeInGameDataCompileItems']", NS)
        self.assertEqual(refresh.get('Condition'), variant.get('Condition'))
        self.assertEqual(set(refresh.get('BeforeTargets').split(';')), {'BeforeClCompile', 'SelectClCompile'})
        self.assertTrue(any(item.get('Remove') == '@(ClCompile)' and 'InGameDataGenerated' in item.get('Condition', '')
                            for item in refresh.findall('m:ItemGroup/m:ClCompile', NS)))

    def test_ingame_project_graph_and_custom_sources(self):
        data = ET.parse(ROOT / 'GameData/GameData.vcxproj').getroot()
        references = data.findall('.//m:ProjectReference', NS)
        self.assertEqual([item.get('Include') for item in references], ['..\\ConsoleGameCore\\ConsoleGameCore.vcxproj'])
        self.assertEqual(references[0].findtext('m:LinkLibraryDependencies', namespaces=NS), 'true')
        core = ET.parse(ROOT / 'ConsoleGameCore/ConsoleGameCore.vcxproj').getroot()
        self.assertFalse(core.findall('.//m:ProjectReference', NS), 'content-independent core must remain the graph root')
        for relative in ('GameData/GameData.vcxproj', 'Tools/GameDataPacker/GameDataPacker.vcxproj'):
            project = ET.parse(ROOT / relative).getroot()
            includes = project.findtext('m:ItemDefinitionGroup/m:ClCompile/m:AdditionalIncludeDirectories', namespaces=NS)
            self.assertIn('ConsoleGameCore', includes)
        for relative, prefix in (('TextRPG/TextRPG.vcxproj', ''), ('NativeTests.vcxproj', 'TextRPG\\')):
            project = ET.parse(ROOT / relative).getroot()
            for kind, extension in (('ClCompile', '.cpp'), ('ClInclude', '.h')):
                name = prefix + 'CustomInGameData\\FCharacterInGameData' + extension
                items = [item for item in project.findall('.//m:' + kind, NS) if item.get('Include') == name]
                self.assertEqual(len(items), 1)
                if kind == 'ClCompile':
                    self.assertIsNone(items[0].find('m:PrecompiledHeaderFile', NS))

    def test_game_owned_sources_share_the_root_pch(self):
        names = ('InGameData\\InGameData.cpp', 'CustomInGameData\\FCharacterInGameData.cpp',
                 'InGameData\\FSaveDataStore.cpp')
        game = ET.parse(ROOT / 'TextRPG/TextRPG.vcxproj').getroot()
        native = ET.parse(ROOT / 'NativeTests.vcxproj').getroot()
        creators = [item.get('Include') for item in registered_items(game, 'ClCompile')
                    if item.findtext('m:PrecompiledHeader', namespaces=NS) == 'Create']
        self.assertEqual(creators, ['pch.cpp'])
        self.assertFalse((ROOT / 'TextRPG/GameDataPch.cpp').exists())
        for name in names:
            for project, prefix in ((game, ''), (native, 'TextRPG\\')):
                item = next(item for item in registered_items(project, 'ClCompile')
                            if item.get('Include') == prefix + name)
                for metadata in ('PrecompiledHeader', 'PrecompiledHeaderFile', 'PrecompiledHeaderOutputFile'):
                    self.assertIsNone(item.find('m:' + metadata, NS), (prefix + name, metadata))
            source = ROOT / 'TextRPG' / name.replace('\\', '/')
            self.assertEqual(source.read_text(encoding='utf-8-sig').splitlines()[0], '#include "pch.h"')
        save = ROOT / 'Tests/SaveData/Main.cpp'
        self.assertEqual(save.read_text(encoding='utf-8-sig').splitlines()[0], '#include "pch.h"')
        self.assertFalse(native.findall("m:ItemDefinitionGroup/m:ClCompile/m:PrecompiledHeaderFile[.='GameData/pch.h']", NS))
        creator = next(item for item in registered_items(native, 'ClCompile')
                       if item.get('Include') == 'TextRPG\\pch.cpp')
        self.assertEqual(creator.findtext('m:PrecompiledHeader', namespaces=NS), 'Create')
        self.assertIn("'$(NativeTestSuite)'=='SaveData'", creator.get('Condition', ''))
        self.assertIn("'$(NativeTestSuite)'=='InGameData'", creator.get('Condition', ''))

    def test_retired_loose_root_and_unused_input_state_are_absent(self):
        header = (ROOT / 'TextRPG/System/GameDataPaths.h').read_text(encoding='utf-8-sig')
        source = (ROOT / 'TextRPG/System/GameDataPaths.cpp').read_text(encoding='utf-8-sig')
        self.assertIn('ResolveGameDataPackagePath', header)
        self.assertIn('ResolveGameDataPackagePath', source)
        for text in (header, source):
            self.assertNotIn('ResolveGameDataRoot', text)
            self.assertNotIn('TEXTRPG_GAMEDATA_ROOT', text)
        self.assertFalse((ROOT / 'Tools/Tests/test_gamedata_paths_analysis.py').exists())
        for relative in ('ConsoleGameCore/Input/InputTypes.h', 'ConsoleGameCore/Input/FConsoleInput.cpp'):
            self.assertNotIn('MouseLeftPressed', (ROOT / relative).read_text(encoding='utf-8-sig'))

    def test_solution_orders_packer_before_game(self):
        solution = (ROOT / "TextRPG.sln").read_text(encoding="utf-8-sig")
        game = solution.split('= "TextRPG",', 1)[1].split("EndProject\n", 1)[0]
        self.assertIn(f"{PACKER_GUID} = {PACKER_GUID}", game)

    def test_direct_project_build_has_non_linking_packer_reference(self):
        game = ET.parse(ROOT / "TextRPG/TextRPG.vcxproj").getroot()
        references = game.findall(".//m:ProjectReference", NS)
        packer = next((r for r in references if r.get("Include", "").endswith("GameDataPacker.vcxproj")), None)
        self.assertIsNotNone(packer, "direct game build must build the packer first")
        self.assertEqual(packer.findtext("m:Project", namespaces=NS), PACKER_GUID)
        self.assertEqual(packer.findtext("m:LinkLibraryDependencies", namespaces=NS), "false")
        self.assertEqual(packer.findtext("m:ReferenceOutputAssembly", namespaces=NS), "false")

    def test_after_build_packages_client_only_next_to_executable(self):
        game = ET.parse(ROOT / "TextRPG/TextRPG.vcxproj").getroot()
        target = game.find("m:Target[@Name='PackGameDataAfterBuild']", NS)
        self.assertIsNotNone(target, "successful game builds must publish GameData.dat")
        self.assertEqual(target.get("AfterTargets"), "Build")
        self.assertIn("DesignTimeBuild", target.get("Condition", ""))
        commands = [e.get("Command", "") for e in target.findall("m:Exec", NS)]
        self.assertEqual(len(commands), 2)
        self.assertIn("prepare-development-key", commands[0])
        self.assertIn(' pack ', commands[1])
        self.assertIn('GameData\\Data\\Client', commands[1])
        self.assertIn('$(TargetDir)GameData.dat', commands[1])
        self.assertIn('--asset-root "$(MSBuildThisFileDirectory)..\\GameData\\Asset"', commands[1])
        self.assertEqual(commands[1].count('--asset-root'), 1)
        for forbidden in ('Schema', 'Excel', 'Class'):
            self.assertNotIn(forbidden, commands[1])
        for execution in target.findall("m:Exec", NS):
            self.assertNotEqual(execution.get("IgnoreExitCode"), "true")

    def test_shared_key_source_and_crypto_link_through_one_library(self):
        data = ET.parse(ROOT / "GameData/GameData.vcxproj").getroot()
        sources = [e.get("Include", "") for e in data.findall(".//m:ClCompile", NS)]
        for name in ("FGameDataKey.cpp", "FGameDataPackage.cpp", "FGameDataStore.cpp", "FGeneratedYaml.cpp"):
            self.assertEqual(sum(s.endswith(name) for s in sources), 1)
        for relative in ("TextRPG/TextRPG.vcxproj", "Tools/GameDataPacker/GameDataPacker.vcxproj"):
            project = ET.parse(ROOT / relative).getroot()
            implementations = [e.get("Include", "") for e in project.findall(".//m:ClCompile", NS)]
            self.assertFalse(any(s.endswith(("FGameDataKey.cpp", "FGameDataPackage.cpp", "FGameDataStore.cpp", "FGeneratedYaml.cpp")) for s in implementations))
            reference = next(e for e in project.findall(".//m:ProjectReference", NS)
                             if e.get("Include", "").endswith("GameData.vcxproj"))
            self.assertEqual(reference.findtext("m:LinkLibraryDependencies", namespaces=NS), "true")
            dependencies = ";".join(e.text or "" for e in project.findall(".//m:AdditionalDependencies", NS))
            self.assertIn("crypt32.lib", dependencies)
            self.assertIn("bcrypt.lib", dependencies)

    def test_all_native_targets_own_isolated_matching_pch_and_filters(self):
        targets = {
            'TextRPG/TextRPG.vcxproj': ('pch.cpp', 'InGameData\\InGameData.cpp',
                                      'CustomInGameData\\FCharacterInGameData.cpp', 'InGameData\\FSaveDataStore.cpp'),
            'Tools/GameDataPacker/GameDataPacker.vcxproj': ('pch.cpp', 'GameDataPacker.cpp'),
            'GameData/GameData.vcxproj': ('Runtime\\GameData\\pch.cpp', 'Runtime\\GameData\\FGameDataKey.cpp',
                                        'Runtime\\GameData\\FGameDataPackage.cpp', 'Runtime\\GameData\\FGameDataStore.cpp',
                                        'Runtime\\GameData\\FGeneratedYaml.cpp'),
            'ConsoleGameCore/ConsoleGameCore.vcxproj': ('pch.cpp', 'Input\\FInputManager.cpp'),
        }
        for relative, names in targets.items():
            with self.subTest(project=relative):
                path = ROOT / relative
                project = ET.parse(path).getroot()
                filters = ET.parse(str(path) + '.filters').getroot()
                items = {e.get('Include'): e for e in project.findall('.//m:ClCompile', NS) if e.get('Include')}
                defaults = next(item for item in project.findall('m:ItemDefinitionGroup/m:ClCompile', NS)
                                if item.find('m:PrecompiledHeaderFile', NS) is not None)
                creator = items[names[0]]
                self.assertEqual(creator.findtext('m:PrecompiledHeader', namespaces=NS), 'Create')
                pch_name = 'pch.h' if relative.startswith(('ConsoleGameCore/', 'TextRPG/')) else 'GameData/pch.h'
                for name in names:
                    item = items[name]
                    self.assertEqual(item.findtext('m:PrecompiledHeaderFile', namespaces=NS)
                                     or defaults.findtext('m:PrecompiledHeaderFile', namespaces=NS), pch_name)
                    if name != names[0]:
                        self.assertEqual(item.findtext('m:PrecompiledHeader', namespaces=NS)
                                         or defaults.findtext('m:PrecompiledHeader', namespaces=NS), 'Use')
                if relative.startswith('GameData/'):
                    self.assertEqual(creator.findtext('m:ObjectFileName', namespaces=NS), '$(IntDir)GameDataPch.obj')
                for kind in ('ClCompile', 'ClInclude'):
                    registered = expanded_paths(path.parent, registered_items(project, kind))
                    linked = expanded_paths(path.parent, registered_items(filters, kind))
                    self.assertEqual(len(registered), len(set(registered)))
                    self.assertEqual(len(linked), len(set(linked)))
                    self.assertEqual(set(registered), set(linked))
                    self.assertTrue(all((path.parent / name).is_file() for name in registered))
                self.assertFalse(project.findall('.//m:ForcedIncludeFiles', NS))
                self.assertTrue(any('RunCodeAnalysis' in e.get('Condition', '') and 'Analysis' in (e.text or '')
                                    for e in project.findall('.//m:IntDir', NS)))
                if relative.startswith('TextRPG/'):
                    self.assertEqual(items['pch.cpp'].findtext('m:PrecompiledHeaderFile', namespaces=NS), 'pch.h')
                    self.assertEqual(items['pch.cpp'].findtext('m:PrecompiledHeader', namespaces=NS), 'Create')
                    self.assertIsNone(items['Scene\\FDataScene.cpp'].find('m:PrecompiledHeaderFile', NS))
                    for group in project.findall('m:ItemDefinitionGroup', NS):
                        if '$(Configuration)|$(Platform)' in group.get('Condition', ''):
                            self.assertEqual(group.findtext('m:ClCompile/m:PrecompiledHeader', namespaces=NS), 'Use')
                            self.assertEqual(group.findtext('m:ClCompile/m:PrecompiledHeaderFile', namespaces=NS), 'pch.h')

    def test_product_references_isolate_consumer_output_globals_not_analysis(self):
        for relative in ('TextRPG/TextRPG.vcxproj', 'Tools/GameDataPacker/GameDataPacker.vcxproj'):
            project = ET.parse(ROOT / relative).getroot()
            references = project.findall('.//m:ProjectReference', NS)
            self.assertTrue(references)
            for reference in references:
                with self.subTest(project=relative, reference=reference.get('Include')):
                    removed = set((reference.findtext('m:GlobalPropertiesToRemove', namespaces=NS) or '').split(';'))
                    self.assertTrue({'OutDir', 'IntDir', 'TargetName'} <= removed,
                                    'consumer globals must not replace dependency-owned caches/output names')
                    self.assertNotIn('RunCodeAnalysis', removed)
        data = ET.parse(ROOT / 'GameData/GameData.vcxproj').getroot()
        references = data.findall('.//m:ProjectReference', NS)
        self.assertEqual(len(references), 1)
        self.assertTrue(references[0].get('Include', '').endswith('ConsoleGameCore.vcxproj'))
        self.assertEqual(references[0].findtext('m:LinkLibraryDependencies', namespaces=NS), 'true')
        removed = set(references[0].findtext('m:GlobalPropertiesToRemove', namespaces=NS).split(';'))
        self.assertTrue({'OutDir', 'IntDir', 'TargetName', 'NativeGeneratedRoot', 'NativeTestSuite'} <= removed)
        self.assertNotIn('RunCodeAnalysis', removed)
        generation = data.find("m:Target[@Name='GenerateGameDataBeforeCompile']", NS)
        for execution in generation.findall('m:Exec', NS):
            command = execution.get('Command')
            self.assertIn('dotnet run --project', command)
            self.assertIn('--configuration $(Configuration)', command)
            self.assertNotIn('$(OutDir)', command)
            self.assertNotIn('$(IntDir)', command)
            self.assertNotIn('$(TargetName)', command)

    def test_game_data_project_and_filters_register_all_owned_files(self):
        folder = ROOT / 'GameData'
        excluded = {'GameData.vcxproj', 'GameData.vcxproj.filters'}
        expected = {str(p.relative_to(folder)) for p in folder.rglob('*')
                    if p.is_file() and p.name not in excluded and not p.name.startswith('~$')
                    and not any(part in {'Out', 'bin', 'obj', '__pycache__'} for part in p.relative_to(folder).parts)}
        registered = {}
        for suffix in ('', '.filters'):
            project = ET.parse(folder / ('GameData.vcxproj' + suffix)).getroot()
            items = [e for kind in ('ClCompile', 'ClInclude', 'None') for e in registered_items(project, kind)]
            paths = expanded_paths(folder, items)
            self.assertEqual(len(paths), len(set(paths)))
            self.assertEqual(expected, set(paths))
            registered[suffix] = {path: e.tag for e in items for path in expanded_paths(folder, [e])}
            if suffix:
                directories = {str(p.parent) for path in expected for p in [Path(path)] if str(p.parent) != '.'}
                for directory in list(directories):
                    directories.update(str(p) for p in Path(directory).parents if str(p) != '.')
                filters = {e.get('Include') for e in project.findall('m:ItemGroup/m:Filter', NS)}
                self.assertEqual(directories, filters)
                for item in items:
                    for path in expanded_paths(folder, [item]):
                        parent = str(Path(path).parent)
                        self.assertEqual('' if parent == '.' else parent, item.findtext('m:Filter', default='', namespaces=NS))
        self.assertEqual(registered[''], registered['.filters'])

    def test_packer_direct_build_outputs_do_not_require_solution_dir(self):
        packer = ET.parse(ROOT / "Tools/GameDataPacker/GameDataPacker.vcxproj").getroot()
        out = packer.findtext(".//m:OutDir", namespaces=NS)
        intermediate = packer.findtext(".//m:IntDir", namespaces=NS)
        self.assertIn("$(MSBuildThisFileDirectory)", out)
        self.assertIn("$(MSBuildThisFileDirectory)", intermediate)
        self.assertNotIn("$(SolutionDir)", out)
        self.assertNotIn("$(SolutionDir)", intermediate)


if __name__ == "__main__":
    unittest.main(verbosity=2)
