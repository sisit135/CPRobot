# SPDX-License-Identifier: GPL-3.0-only
import copy
import json
from pathlib import Path
import re
import shutil
import tempfile
import unittest
from google.protobuf import descriptor_pb2
import generate_schema as generator


class SchemaGeneration(unittest.TestCase):
    def test_descriptor_catalog_and_firmware_bytes_agree(self):
        outputs = generator.generate()
        data = outputs['fw/robot_sf/proto/settings.desc']
        desc = descriptor_pb2.FileDescriptorSet.FromString(data)
        message = next(m for f in desc.file for m in f.message_type if m.name == 'FlashSettings')
        manifest = json.loads((generator.ROOT / 'fw/robot_sf/proto/flash_settings.json').read_text())
        fields = sorted(manifest['fields'], key=lambda field: field['key'])
        self.assertEqual([(f.number, f.name) for f in message.field],
                         [(f['key'], f['name']) for f in fields])
        self.assertEqual(len(message.field), 72)
        blob = outputs['fw/robot_sf/include/remote/schema_blob.hpp'].decode().split('schema_blob[] = {')[1].split('}')[0]
        self.assertEqual(bytes(int(v) for v in re.findall(r'\d+', blob)), data)
        spec = next(m for f in desc.file for m in f.message_type if m.name == 'FieldSpec')
        self.assertIn(('default_value', 8), [(f.name, f.number) for f in spec.field])
        self.assertIn(('source_id', 9), [(f.name, f.number) for f in spec.field])
        self.assertEqual(outputs, generator.generate())

    def invalid_manifest(self, change):
        manifest = json.loads((generator.ROOT / 'fw/robot_sf/proto/flash_settings.json').read_text())
        change(manifest)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'fw/robot_sf/proto').mkdir(parents=True)
            (root / 'fw/robot_sf/proto/flash_settings.json').write_text(json.dumps(manifest))
            for source in manifest['sources'].values():
                target = root / 'fw/robot_sf/include' / source
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(generator.ROOT / 'fw/robot_sf/include' / source, target)
            with self.assertRaises(ValueError):
                generator.generate(root)

    def test_duplicate_key_rejected(self):
        self.invalid_manifest(lambda m: m['fields'].append(copy.deepcopy(m['fields'][0])))

    def test_default_outside_limits_rejected(self):
        self.invalid_manifest(lambda m: m['fields'][0].update(default=2))

    def test_nonintegral_integer_rejected(self):
        self.invalid_manifest(lambda m: m['fields'][0].update(default=.5))

    def test_nonfinite_limits_rejected(self):
        self.invalid_manifest(lambda m: m['fields'][0].update(maximum=float('inf')))


if __name__ == '__main__':
    unittest.main()
