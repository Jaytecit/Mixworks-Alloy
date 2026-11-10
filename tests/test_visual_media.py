"""Media roles are based on alpha content, with exact duplicate identities."""
import sys,tempfile,unittest
from pathlib import Path
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from visual_media import image_role,content_identity,sprite_pixels

class VisualMediaTests(unittest.TestCase):
    def test_nearly_transparent_quantisation_noise_cannot_seed_sprite_material(self):
        import numpy as np
        source=np.array([[[255,0,0,1],[255,255,0,5],[170,85,85,9],[191,127,63,12],
                          [40,80,120,64],[40,80,120,255]]],dtype=np.uint8)
        clean=source.copy();clean[0,:4]=0
        self.assertTrue(np.array_equal(sprite_pixels(source),sprite_pixels(clean)))
        self.assertTrue(np.all(sprite_pixels(source)[0,:4]==0))

    def test_hidden_colours_do_not_change_visible_sprite_preparation(self):
        import numpy as np
        a=np.array([[[30,90,120,128],[255,0,0,0]]],dtype=np.uint8)
        b=a.copy();b[0,1,:3]=(0,255,255)
        self.assertTrue(np.array_equal(sprite_pixels(a),sprite_pixels(b)))
        self.assertTrue(np.all(sprite_pixels(a)[0,1]==0))

    def test_faint_matte_does_not_change_opaque_artwork_exposure(self):
        import numpy as np
        clean=np.zeros((32,32,4),dtype=np.uint8)
        clean[8:16,8:16]=(64,96,72,255)
        matte=clean.copy()
        matte[clean[:,:,3]==0]=(255,220,255,24)
        self.assertTrue(np.array_equal(sprite_pixels(clean)[8:16,8:16],
                                       sprite_pixels(matte)[8:16,8:16]))

    def test_translucent_material_preserves_coverage_hue_and_premultiplied_bounds(self):
        import numpy as np
        source=np.array([[[40,80,120,64],[40,80,120,128],[40,80,120,255]]],dtype=np.uint8)
        prepared=sprite_pixels(source)
        baseline=source[:,:,:3].astype(float)*source[:,:,3:4]/255
        self.assertTrue(np.all(prepared[:,:,:3]>=baseline-1))
        self.assertTrue(np.array_equal(prepared[:,:,3],source[:,:,3]))
        self.assertTrue(np.all(prepared[:,:,:3]<=prepared[:,:,3:4]))
        # Exposure changes intensity, preserving the original 1:2:3 hue.
        self.assertAlmostEqual(float(prepared[0,2,0]/prepared[0,2,2]),1/3,delta=.02)
        self.assertAlmostEqual(float(prepared[0,2,1]/prepared[0,2,2]),2/3,delta=.02)

    def test_black_detail_and_bright_opaque_sprite_pixels_remain_intact(self):
        import numpy as np
        source=np.array([[[0,0,0,255],[255,255,255,255]]],dtype=np.uint8)
        self.assertTrue(np.array_equal(sprite_pixels(source),source))

    def test_opaque_png_is_background_and_alpha_object_is_cutout(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'image.png';Image.new('RGBA',(32,32),(12,40,50,255)).save(path)
            self.assertEqual(image_role(path),'background')
            image=Image.new('RGBA',(32,32),(0,0,0,0));image.paste((240,100,20,255),(8,4,24,28));image.save(path)
            self.assertEqual(image_role(path),'cutout')
    def test_duplicate_identity_ignores_filename(self):
        with tempfile.TemporaryDirectory() as d:
            a=Path(d)/'background.png';b=Path(d)/'sprite.png'
            Image.new('RGB',(16,16),'red').save(a);b.write_bytes(a.read_bytes())
            self.assertEqual(content_identity(a),content_identity(b))
            Image.new('RGB',(16,16),'blue').save(b)
            self.assertNotEqual(content_identity(a),content_identity(b))
    def test_invisible_and_corrupt_images_are_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'bad.png';Image.new('RGBA',(8,8),(1,2,3,0)).save(p)
            with self.assertRaises(ValueError):image_role(p)
            p.write_bytes(b'not an image')
            with self.assertRaises(Exception):image_role(p)

if __name__=='__main__':unittest.main()
