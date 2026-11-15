"""현재 TextArt 자산과 Scene.xlsx UI 계약의 읽기 전용 회귀 검사.

실행: openpyxl이 설치된 Python으로 python Tools/Tests/test_scene_ui_data.py
생성기 거부/출력 보호 검사는 GameDataGenerator --self-test에서 수행한다.
"""
from pathlib import Path
import hashlib
import re
import os
import unittest

from openpyxl import load_workbook


ROOT = Path(__file__).resolve().parents[2]
ASSETS = (
    ("MainLogo", "Asset/Logo/Logo_Text_Rpg.txt", 83, 41),
    ("PlayerSwordswoman", "Asset/Character/Character_Swordswoman.txt", 36, 36),
    ("MonsterDragon", "Asset/Monster/Monster_Dragon.txt", 36, 36),
    ("TownWindwood", "Asset/Town/Town_Windwood.txt", 36, 36),
)
# 승인된 기본 네 자산의 원시 바이트만 고정하며 일반 카탈로그 형식 검증과 구분한다.
APPROVED_ASSET_SHA256 = {
    "Asset/Logo/Logo_Text_Rpg.txt": "c294988f6075b0eb14f9b8f18e261e67cb0ee2f595bf314ab86ab22fb97a17d6",
    "Asset/Character/Character_Swordswoman.txt": "9ee19a996a0fc8d5c8f945346740da90ae30c69f8e4e77ff8c4ffe2a2d1f80d3",
    "Asset/Monster/Monster_Dragon.txt": "0be8e7cf71d3e1d2c84afad1d092fd765c128eee992cea474af04e8df2cecc67",
    "Asset/Town/Town_Windwood.txt": "7fe35ff71bdbc6bb47af8b429d2edc063ed3bf01add6ddd61932644ab611714f",
}


def is_supported_art_glyph(character):
    code = ord(character)
    return (0x20 <= code <= 0x7E or 0x2500 <= code <= 0x259F
            or 0x2190 <= code <= 0x2193 or code in {0x25B2, 0x25B6, 0x25BC, 0x25C0, 0x25CB})


def table_records(sheet):
    fields = {sheet.cell(13, column).value: column for column in range(2, sheet.max_column + 1)}
    return [
        {name: sheet.cell(row, column).value for name, column in fields.items()}
        for row in range(14, sheet.max_row + 1)
        if any(sheet.cell(row, column).value is not None for column in fields.values())
    ]


class SceneUiDataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        workbook_path = Path(os.environ.get("TEXTRPG_TEST_WORKBOOK", ROOT / "GameData/Excel/Scene.xlsx")).resolve(strict=True)
        cls.workbook = load_workbook(workbook_path)
        cls.data_root = workbook_path.parent.parent

    @classmethod
    def tearDownClass(cls):
        cls.workbook.close()

    def assert_art_dimensions(self, path, width, height):
        self.assertTrue(path.is_file(), f"missing TextArt asset: {path}")
        text = path.read_bytes().decode("utf-8-sig", errors="strict")
        lines = text.replace("\r\n", "\n").split("\n")
        if lines[-1] == "":
            lines.pop()
        self.assertEqual(height, len(lines))
        for row, line in enumerate(lines, 1):
            self.assertEqual(width, len(line), f"{path}: row {row}; trailing spaces are significant")
            self.assertTrue(all(map(is_supported_art_glyph, line)), f"{path}: unsupported glyph in row {row}")

    def test_four_assets_preserve_catalog_ids_paths_and_dimensions(self):
        catalog = {row["ArtId"]: row for row in table_records(self.workbook["TextArt"])}
        for art_id, path, width, height in ASSETS:
            with self.subTest(art_id=art_id):
                self.assertIn(art_id, catalog)
                self.assertEqual((path, width, height),
                                 (catalog[art_id]["AssetPath"], catalog[art_id]["Width"], catalog[art_id]["Height"]))
                self.assert_art_dimensions(self.data_root / path, width, height)
                self.assertEqual(APPROVED_ASSET_SHA256[path],
                                 hashlib.sha256((self.data_root / path).read_bytes()).hexdigest(),
                                 f"{path}: approved default TextArt raw-byte SHA256 mismatch")

    def test_asset_filenames_use_category_and_title_words(self):
        assets = self.data_root / "Asset"
        paths = [path for path in assets.rglob("*") if path.is_file()]
        self.assertTrue(paths)
        for path in paths:
            with self.subTest(asset=path.relative_to(assets).as_posix()):
                category = path.relative_to(assets).parts[0]
                words = path.stem.split("_")
                self.assertGreater(len(words), 1)
                self.assertEqual(category, words[0])
                self.assertEqual(path.suffix.lower(), path.suffix)
                for word in words[1:]:
                    self.assertRegex(word, r"^(?:[A-Z][a-z0-9]*|[0-9]+x[0-9]+)$")

    def test_every_scene_type_has_exactly_four_ordered_options_including_disabled(self):
        options = table_records(self.workbook["SceneOption"])
        for scene in table_records(self.workbook["Scene"]):
            with self.subTest(scene=scene["SceneId"]):
                rows = [row for row in options if row["SceneType"] == scene["SceneType"]]
                self.assertEqual(4, len(rows))
                self.assertEqual([1, 2, 3, 4], sorted(row["OptionOrder"] for row in rows))

    def test_every_config_is_four_without_paging_or_scroll(self):
        configurations = table_records(self.workbook["SceneTypeConfig"])
        for config in configurations:
            with self.subTest(scene_type=config["SceneType"]):
                self.assertEqual(4, config["MaxVisibleOptions"])
                self.assertEqual("NoScroll", config["OptionOverflowPolicy"])
        self.assertEqual({row["SceneType"] for row in table_records(self.workbook["Scene"])},
                         {row["SceneType"] for row in configurations})

    def test_scene_definition_contains_only_static_bootstrap_fields(self):
        sheet = self.workbook["Scene"]
        fields = [sheet.cell(13, column).value for column in range(2, sheet.max_column + 1)]
        self.assertEqual(["SceneId", "SceneType", "IsStartupScene"], fields)
        forbidden = {"PlayerArtId", "ContentArtId", "TitleTextKey", "RegionTextKey"}
        for scene in table_records(sheet):
            self.assertTrue(forbidden.isdisjoint(scene))

    def test_text_art_catalog_is_independent_of_scene_definitions(self):
        self.assertIn("TextArt", self.workbook.sheetnames)
        rows = table_records(self.workbook["TextArt"])
        identifiers = [row["ArtId"] for row in rows]
        self.assertEqual(len(identifiers), len(set(identifiers)))
        for art in rows:
            with self.subTest(art=art["ArtId"]):
                self.assertTrue(art["ArtId"])
                self.assertIn((art["Width"], art["Height"]), {(36, 36), (83, 41)})
                self.assert_art_dimensions(self.data_root / art["AssetPath"], art["Width"], art["Height"])

    def test_operational_labels_are_bilingual_and_referenced(self):
        strings = {row["TextKey"]: row for row in table_records(self.workbook["Localization"])}
        for row in strings.values():
            self.assertTrue(row["KoKR"] and row["EnUS"])
        for option in table_records(self.workbook["SceneOption"]):
            key = option["OptionTextKey"]
            self.assertIn(key, strings)
            self.assertTrue(strings[key]["KoKR"] and strings[key]["EnUS"])

    def test_unimplemented_actions_are_visible_but_disabled(self):
        for option in table_records(self.workbook["SceneOption"]):
            self.assertEqual(option["ActionType"] in {"StartGame", "Quit"}, option["IsEnabled"])
        main = sorted((row for row in table_records(self.workbook["SceneOption"]) if row["SceneType"] == "MainMenu"),
                      key=lambda row: row["OptionOrder"])
        self.assertEqual([1, 2, 3, 5], [row["OptionId"] for row in main])
        self.assertEqual(["StartGame", "Continue", "Settings", "Quit"], [row["ActionType"] for row in main])

    def test_new_assets_are_registered_in_matching_visual_studio_filters(self):
        from xml.etree import ElementTree

        project = ROOT / "TextRPG/TextRPG.vcxproj"
        namespace = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
        project_items = {node.attrib["Include"] for node in ElementTree.parse(project).findall(".//m:None", namespace)}
        filters = ElementTree.parse(str(project) + ".filters")
        definitions = {node.attrib["Include"] for node in filters.findall(".//m:Filter[@Include]", namespace)}
        filtered_items = {node.attrib["Include"]: node.findtext("m:Filter", namespaces=namespace)
                          for node in filters.findall(".//m:None", namespace)}
        for _, path, _, _ in ASSETS:
            with self.subTest(path=path):
                include = "..\\GameData\\" + path.replace("/", "\\")
                expected_filter = "GameData\\" + str(Path(path).parent).replace("/", "\\")
                self.assertIn(include, project_items)
                self.assertEqual(expected_filter, filtered_items[include])
                self.assertIn(expected_filter, definitions)

    def test_solution_orders_generator_before_native_generation(self):
        solution = (ROOT / "TextRPG.sln").read_text(encoding="utf-8-sig")
        generator = re.search(r'Project\([^\n]+\) = "GameDataGenerator",[^\n]+"(\{[^}]+\})"', solution)
        native = re.search(r'Project\([^\n]+\) = "TextRPG",[^\n]+\n(.*?)EndProject', solution, re.DOTALL)
        self.assertIsNotNone(generator)
        self.assertIsNotNone(native)
        self.assertIn("ProjectSection(ProjectDependencies)", native.group(1))
        self.assertIn(f"{generator.group(1)} = {generator.group(1)}", native.group(1))

    def test_project_explicitly_compiles_utf8_sources_with_matching_pch_charset(self):
        from xml.etree import ElementTree

        namespace = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
        tree = ElementTree.parse(ROOT / "TextRPG/TextRPG.vcxproj")
        shared_options = [node.text or "" for group in tree.findall("m:ItemDefinitionGroup", namespace)
                          if not group.attrib.get("Condition")
                          for node in group.findall("m:ClCompile/m:AdditionalOptions", namespace)]
        self.assertTrue(any("/utf-8" in value.split() for value in shared_options))

    def test_scene_definitions_and_assets_have_no_legacy_layout_dependency(self):
        self.assertFalse((self.data_root / "Asset/Scene").exists())
        for scene in table_records(self.workbook["Scene"]):
            self.assertNotIn("AssetPath", scene)


if __name__ == "__main__":
    unittest.main(verbosity=2)
