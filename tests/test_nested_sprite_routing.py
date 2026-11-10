"""Sprites are explicit scene geometry inputs; the three relief methods are independent."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from visual_catalogue import catalogue


class NestedSpriteTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.entries=catalogue(Path(__file__).resolve().parents[1]/'visuals/original')

    def test_flat_effects_do_not_receive_geometry_sprites(self):
        for slug in ('prism-datastream','chroma-monsoon','copper-silt','image-gallery','prism-rosarium'):
            self.assertFalse(self.entries['original/'+slug]['spriteGeometry'],slug)

    def test_geometry_contract_is_explicit(self):
        self.assertEqual(self.entries['original/molten-contours']['spriteGeometry'],'relief')
        self.assertEqual(self.entries['original/firefly-assembly-colour-separation']['spriteGeometry'],'particles')
        self.assertEqual(self.entries['original/chromatic-torso']['spriteGeometry'],'surface')

    def test_each_relief_method_has_an_individual_preset(self):
        names=('reliefDepth','meltWarp','displacementColour')
        materials=[e for e in self.entries.values() if 'reliefDepth' in e['controls']]
        self.assertEqual(len(materials),33)
        for entry in materials:
            with self.subTest(scene=entry['key']):
                presets={p['id']:p['controls'] for p in entry['presets']}
                for enabled,preset in enumerate(('relief-only','warp-only','colour-only')):
                    values=[presets[preset][name] for name in names]
                    self.assertGreater(values[enabled],0)
                    self.assertTrue(all(value==0 for i,value in enumerate(values) if i!=enabled))
                self.assertTrue(all(presets['all-methods'][name]>0 for name in names))

    def test_added_methods_default_off_in_existing_scene_looks(self):
        names=('reliefDepth','meltWarp','displacementColour')
        for entry in self.entries.values():
            if 'reliefDepth' not in entry['controls'] or entry['key']=='original/molten-contours':continue
            with self.subTest(scene=entry['key']):
                self.assertTrue(all(entry['controls'][name]['default']==0 for name in names))
                for preset in entry['presets'][:6]:self.assertTrue(all(preset['controls'][name]==0 for name in names))


if __name__=='__main__':unittest.main()
