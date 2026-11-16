"""프로젝트 소유 일반 헤더의 include 금지와 PCH 경계를 검사한다."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]', re.MULTILINE)

class HeaderIncludePolicyTests(unittest.TestCase):
    def test_scene_api_has_no_loose_data_root_parameter(self):
        # 소스 계약만 검사한다. native 컴파일/런타임 검증은 별도 gate다.
        for relative in ('Scene/FDataScene.h', 'Scene/FDataScene.cpp',
                         'Scene/FSceneManager.h', 'Scene/FSceneManager.cpp'):
            with self.subTest(source=relative):
                text = (ROOT / 'TextRPG' / relative).read_text(encoding='utf-8-sig')
                self.assertNotIn('std::filesystem::path', text)
                self.assertNotIn('gameDataRoot', text)

    def test_scene_and_text_art_do_not_retain_write_only_copies(self):
        for relative, member in (('Scene/FDataScene', 'm_options'), ('UI/Common/FUiTextArt', 'm_artId')):
            for extension in ('.h', '.cpp'):
                with self.subTest(source=relative + extension):
                    text = (ROOT / 'TextRPG' / (relative + extension)).read_text(encoding='utf-8-sig')
                    self.assertNotRegex(text, rf'\b{member}\b')
        scene = (ROOT / 'TextRPG/Scene/FDataScene.h').read_text(encoding='utf-8-sig')
        self.assertIn('std::uint32_t OptionId = 0;', scene)

    def test_authored_headers_have_no_includes_except_pch(self):
        paths = [path for folder in ['TextRPG', 'ConsoleGameCore', 'GameData/Runtime', 'Tools', 'Tests']
                 for path in (ROOT / folder).rglob('*')
                 if path.suffix.lower() in {'.h', '.hpp', '.hxx'}
                 and not any(part in {'Out', 'bin', 'obj', '__pycache__'} for part in path.parts)
                 and path.name != 'pch.h'
                 and path.relative_to(ROOT).as_posix() not in {
                     'ConsoleGameCore/ConsoleGameCorePch.h', 'GameData/Runtime/GameData/GameDataPch.h'}]
        self.assertTrue(paths)
        violations = [str(path.relative_to(ROOT)) for path in paths
                      if INCLUDE.search(path.read_text(encoding='utf-8-sig'))]
        self.assertEqual([], violations)

    def test_generated_headers_have_no_includes(self):
        for folder, aggregate in (('GameData', 'GameDataAll.h'), ('InGameData', 'InGameDataAll.h')):
            paths = [p for p in (ROOT / 'GameData/Generated' / folder).rglob('*.h') if p.name != aggregate]
            with self.subTest(family=folder):
                self.assertTrue(paths)
                self.assertEqual([], [str(path.relative_to(ROOT)) for path in paths
                                      if INCLUDE.search(path.read_text(encoding='utf-8-sig'))])

    def test_key_and_yaml_use_shared_pch_before_concrete_definitions(self):
        for filename in ('FGameDataKey.cpp', 'FGeneratedYaml.cpp'):
            with self.subTest(source=filename):
                text = (ROOT / 'GameData/Runtime/GameData' / filename).read_text(encoding='utf-8-sig')
                includes = re.findall(r'^\s*#\s*include\s*([<"][^>"\n]+[>"])', text, re.MULTILINE)
                self.assertEqual(includes, ['"GameData/pch.h"', f'"{Path(filename).stem}.h"'])

    def test_shared_gamedata_pch_contains_only_stable_std_and_sdk(self):
        header = ROOT / 'GameData/Runtime/GameData/GameDataPch.h'
        self.assertTrue(header.is_file(), 'shared GameData PCH header is required')
        text = header.read_text(encoding='utf-8-sig')
        dependencies = set(re.findall(r'^\s*#\s*include\s*<([^>]+)>', text, re.MULTILINE))
        required = {'Windows.h', 'dpapi.h', 'bcrypt.h', 'algorithm', 'array', 'cassert',
                    'charconv', 'cstddef', 'cstdint', 'cstdlib', 'exception', 'filesystem',
                    'fstream', 'functional', 'iostream', 'iterator', 'limits', 'map', 'new', 'set',
                    'span', 'stdexcept', 'string', 'string_view', 'system_error', 'tuple',
                    'unordered_map', 'utility', 'vector'}
        required |= {'bit', 'cmath', 'memory', 'optional', 'type_traits', 'unordered_set'}
        self.assertEqual(required, dependencies)
        authored = re.findall(r'#\s*include\s*"([^"]+)"', text)
        self.assertEqual(authored, ['Serialization/ISerializable.h'])
        self.assertGreater(text.index('#include "Serialization/ISerializable.h"'), text.index('#include <vector>'))
        self.assertNotIn('FSerializationArchive.h', text)
        self.assertNotIn('ConsoleGameCorePch.h', text)
        self.assertEqual(set(re.findall(r'^#define\s+(\w+)', text, re.MULTILINE)),
                         {'NOMINMAX', 'WIN32_LEAN_AND_MEAN'})
        self.assertNotRegex(text, r'\b(?:class|struct|namespace)\s+\w+')
        self.assertEqual((ROOT / 'GameData/Runtime/GameData/pch.cpp').read_text(encoding='utf-8-sig').strip(),
                         '#include "GameData/pch.h"')

    def test_pch_contains_stable_prerequisites_not_concrete_classes(self):
        text = (ROOT / 'ConsoleGameCore/ConsoleGameCorePch.h').read_text(encoding='utf-8-sig')
        for dependency in ['array', 'cstddef', 'cstdint', 'filesystem', 'memory', 'string', 'vector']:
            self.assertIn(f'#include <{dependency}>', text)
        self.assertIn('#include "Objects/IObject.h"', text)
        self.assertIn('#include "UI/Common/IUI.h"', text)
        self.assertIn('#include "UI/IUiInputTarget.h"', text)
        self.assertNotIn('GameData', text)
        self.assertNotIn('Scene/IScene.h', text)
        game = (ROOT / 'TextRPG/pch.h').read_text(encoding='utf-8-sig')
        self.assertIn('#include "GameData/GameDataPch.h"', game)
        self.assertIn('#include "../ConsoleGameCore/ConsoleGameCorePch.h"', game)
        self.assertIn('#include "Scene/IScene.h"', game)
        for content in [text, game]:
            self.assertNotIn('FFrameBuffer.h', content)
            self.assertNotIn('.g.h', content)
            self.assertNotIn('FUIManager.h', content)

    def test_consuming_pch_uses_named_shared_prerequisites_not_library_pch(self):
        game = (ROOT / 'TextRPG/pch.h').read_text(encoding='utf-8-sig')
        self.assertNotIn('"GameData/pch.h"', game)
        self.assertNotIn('"../ConsoleGameCore/pch.h"', game)
        self.assertIn('"GameData/GameDataPch.h"', game)
        self.assertIn('"../ConsoleGameCore/ConsoleGameCorePch.h"', game)
        for relative in ('TextRPG/pch.h', 'ConsoleGameCore/pch.h',
                         'ConsoleGameCore/ConsoleGameCorePch.h',
                         'GameData/Runtime/GameData/pch.h', 'GameData/Runtime/GameData/GameDataPch.h'):
            self.assertNotIn('GameDataAll.h', (ROOT / relative).read_text(encoding='utf-8-sig'))
        self.assertIn('#include "GameDataAll.h"', (ROOT / 'TextRPG/Frame/FTextRpgFrame.cpp').read_text(encoding='utf-8-sig'))

if __name__ == '__main__':
    unittest.main(verbosity=2)
