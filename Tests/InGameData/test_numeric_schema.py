"""Guard the authored content schema independently of generated output."""
from pathlib import Path
import re
import unittest


class NumericSchemaTests(unittest.TestCase):
    def test_content_ids_are_numeric_schema_two(self):
        root = Path(__file__).resolve().parents[2]
        schema = (root / 'GameData/InGameDataSchema/InGameData.yaml').read_text(encoding='utf-8-sig')
        self.assertRegex(schema, r'(?m)^schema_version: 2$')
        for field in ('CharacterId', 'ItemId', 'AchievementId', 'm_sceneId'):
            self.assertRegex(schema, rf'- name: {field}\n\s+type: \'uint32\'')
        self.assertNotIn("type: 'string'", schema)


if __name__ == '__main__':
    unittest.main()
