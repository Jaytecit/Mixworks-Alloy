"""Read-only media role inspection. A file extension does not imply transparency."""
import warnings
import hashlib
from PIL import Image

def content_identity(path):
    with open(path,'rb') as source:return hashlib.file_digest(source,'sha256').hexdigest()

def image_role(path):
    with warnings.catch_warnings():
        warnings.simplefilter('error',Image.DecompressionBombWarning)
        with Image.open(path) as source:
            if source.width*source.height>16_777_216:raise ValueError('Scene image exceeds 16 megapixels')
            rgba=source.convert('RGBA');rgba.thumbnail((128,128))
            hist=rgba.getchannel('A').histogram();count=sum(hist)
            if sum(hist[8:])==0:raise ValueError('Image is fully transparent')
            # Ignore an isolated transparent pixel or an otherwise opaque alpha channel.
            return 'cutout' if sum(hist[:250])/count>.01 else 'background'
