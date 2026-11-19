"""생성물과 독립적으로 숫자 키/현재 진행 스키마 원본을 보호한다."""
from pathlib import Path
import re
import unittest


class NumericSchemaTests(unittest.TestCase):
    def test_content_ids_and_currency_wallet_are_numeric_schema_five(self):
        root = Path(__file__).resolve().parents[2]
        schema = (root / 'GameData/InGameDataSchema/InGameData.yaml').read_text(encoding='utf-8-sig')
        self.assertRegex(schema, r'(?m)^schema_version: 5$')
        for field in ('CharacterId', 'ItemId', 'AchievementId', 'm_sceneId'):
            self.assertRegex(schema, rf'- name: {field}\n\s+type: \'uint32\'')
        self.assertNotIn("type: 'string'", schema)
        self.assertIn("type: 'vector<SCurrencyBalance>'", schema)
        self.assertNotIn('- name: m_gold', schema)


if __name__ == '__main__':
    unittest.main()
