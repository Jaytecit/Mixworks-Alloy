"""Media roles are based on alpha content, with exact duplicate identities."""
import sys,tempfile,unittest
from pathlib import Path
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from visual_media import image_role,content_identity

class VisualMediaTests(unittest.TestCase):
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
