internal static partial class FGameDataValidator
{
    private static void ValidateMovement(IReadOnlyList<FGameDataTable> tables,
        IReadOnlyDictionary<string, FGameDataEnumDefinition> enums, FDiagnosticBag diagnostics)
    {
        foreach (string name in new[] { "Enums", "Map", "Character" })
        {
            if (tables.Count(table => table.Name == name) != 1)
                diagnostics.Add("GD2001", $"Movement requires exactly one {name} table.",
                    new SSourceLocation("movement", name, "B1"));
        }
        if (enums.Count != 1 || !enums.TryGetValue("MapType", out FGameDataEnumDefinition? mapType) ||
            mapType.Target != EGameDataTarget.Shared || mapType.UnderlyingType != EGameDataEnumUnderlyingType.UInt8 ||
            !mapType.Values.SequenceEqual(new SEnumValue[] { new("Town", 1), new("Dungeon", 2) }))
            diagnostics.Add("GD2002", "Movement requires only Shared uint8 MapType: Town=1|Dungeon=2.",
                new SSourceLocation("movement", "Enums", "B14"));

        foreach (FGameDataTable table in tables.Where(table => !FGameDataEnumCatalog.IsDefinitionTable(table)))
        {
            bool isMap = table.Name == "Map";
            if (!isMap && table.Name != "Character")
            {
                diagnostics.Add("GD2001", "Unsupported movement table.", table.Locate(1, 2));
                continue;
            }
            string id = isMap ? "MapDataId" : "CharacterDataId";
            string asset = isMap ? "MapAssetKey" : "SpriteAssetKey";
            var contract = new List<(string Name, EGameDataScalarKind Kind, EGameDataScope Scope)>
            {
                (id, EGameDataScalarKind.UInt32, EGameDataScope.Shared),
                ("Name", EGameDataScalarKind.String, EGameDataScope.Shared),
                (asset, EGameDataScalarKind.String, EGameDataScope.Client),
            };
            if (isMap)
            {
                contract.Add(("MapType", EGameDataScalarKind.Enum, EGameDataScope.Shared));
                foreach (string field in new[] { "WorldWidth", "WorldHeight", "SectorSize", "AoiSectorRadius" })
                    contract.Add((field, EGameDataScalarKind.UInt32, EGameDataScope.Shared));
                contract.Add(("SpawnX", EGameDataScalarKind.Float, EGameDataScope.Server));
                contract.Add(("SpawnY", EGameDataScalarKind.Float, EGameDataScope.Server));
            }
            else
            {
                contract.Add(("MoveSpeed", EGameDataScalarKind.Float, EGameDataScope.Server));
                contract.Add(("CollisionRadius", EGameDataScalarKind.Float, EGameDataScope.Server));
            }
            Dictionary<string, FGameDataField> fields = BuildFieldMap(table);
            if (!ValidateExactFieldSet(table, fields, contract.Select(field => field.Name).ToArray(), "GD2003", diagnostics))
                continue;
            if (table.Target != EGameDataTarget.Shared)
                diagnostics.Add("GD2004", "Movement table must have Shared target.", table.Locate(2, 2));
            foreach (var field in contract)
            {
                ValidateContractField(table, fields[field.Name], field.Kind, field.Scope, "GD2004", diagnostics,
                    mustBePrimaryKey: field.Name == id);
                if (fields[field.Name].IsPrimaryKey != (field.Name == id))
                    diagnostics.Add("GD2004", "Only DataId can be primary key.", fields[field.Name].Location);
            }
            if (isMap) ValidateEnumContractField(table, fields["MapType"], "MapType", EGameDataScope.Shared, "GD2004", diagnostics);
            foreach (FGameDataRow row in table.Rows)
            {
                bool valid = GetUInt32(row, id) > 0 && !string.IsNullOrWhiteSpace(GetString(row, "Name")) &&
                    !string.IsNullOrWhiteSpace(GetString(row, asset));
                if (isMap)
                {
                    uint width = GetUInt32(row, "WorldWidth"), height = GetUInt32(row, "WorldHeight");
                    uint sector = GetUInt32(row, "SectorSize"), aoi = GetUInt32(row, "AoiSectorRadius");
                    float x = GetFloat(row, "SpawnX"), y = GetFloat(row, "SpawnY");
                    valid &= width > 0 && height > 0 && sector > 0 && width % sector == 0 && height % sector == 0 &&
                        aoi is >= 1 and <= 4 && float.IsFinite(x) && float.IsFinite(y) && x >= 0 && y >= 0 && x < width && y < height;
                }
                else
                {
                    float speed = GetFloat(row, "MoveSpeed"), radius = GetFloat(row, "CollisionRadius");
                    valid &= float.IsFinite(speed) && speed > 0 && float.IsFinite(radius) && radius > 0;
                }
                if (!valid) diagnostics.Add("GD2005", "Invalid movement row: ID/name/resource/geometry/range.", table.Locate(row.SourceRowNumber, 2));
            }
        }
    }
}
