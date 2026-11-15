"""MSBuild GameData.dat 배포 계약 검사. 실제 패킹/실행 검증은 별도로 수행한다."""
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
PACKER_GUID = "{E3145A6F-EC7F-46A7-9F63-CC75CC4FE405}"


class GameDataBuildContract(unittest.TestCase):
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
            'TextRPG/TextRPG.vcxproj': ('GameDataPch.cpp',
                                      'InGameData\\InGameData.cpp', 'InGameData\\FSaveDataStore.cpp'),
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
                defaults = project.find('m:ItemDefinitionGroup/m:ClCompile', NS)
                creator = items[names[0]]
                self.assertEqual(creator.findtext('m:PrecompiledHeader', namespaces=NS), 'Create')
                pch_name = 'pch.h' if relative.startswith('ConsoleGameCore/') else 'GameData/pch.h'
                for name in names:
                    item = items[name]
                    self.assertEqual(item.findtext('m:PrecompiledHeaderFile', namespaces=NS)
                                     or defaults.findtext('m:PrecompiledHeaderFile', namespaces=NS), pch_name)
                    if name != names[0]:
                        self.assertEqual(item.findtext('m:PrecompiledHeader', namespaces=NS)
                                         or defaults.findtext('m:PrecompiledHeader', namespaces=NS), 'Use')
                if relative.startswith(('GameData/', 'TextRPG/')):
                    self.assertEqual(creator.findtext('m:ObjectFileName', namespaces=NS), '$(IntDir)GameDataPch.obj')
                for kind in ('ClCompile', 'ClInclude'):
                    registered = [e.get('Include') for e in project.findall('.//m:' + kind, NS) if e.get('Include')]
                    linked = [e.get('Include') for e in filters.findall('.//m:' + kind, NS) if e.get('Include')]
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
        self.assertFalse(data.findall('.//m:ProjectReference', NS))
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
            items = [e for kind in ('ClCompile', 'ClInclude', 'None')
                     for e in project.findall('.//m:' + kind, NS) if e.get('Include')]
            paths = [e.get('Include') for e in items]
            self.assertEqual(len(paths), len(set(paths)))
            self.assertEqual(expected, set(paths))
            registered[suffix] = {e.get('Include'): e.tag for e in items}
            if suffix:
                directories = {str(p.parent) for path in expected for p in [Path(path)] if str(p.parent) != '.'}
                for directory in list(directories):
                    directories.update(str(p) for p in Path(directory).parents if str(p) != '.')
                filters = {e.get('Include') for e in project.findall('m:ItemGroup/m:Filter', NS)}
                self.assertEqual(directories, filters)
                for item in items:
                    parent = str(Path(item.get('Include')).parent)
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
