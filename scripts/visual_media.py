"""Read-only media role inspection. A file extension does not imply transparency."""
import warnings
import hashlib
from PIL import Image

ALPHA_NOISE_MAX = 12
ALPHA_EXPOSURE_MIN = 64


def sprite_pixels(pixels):
    """Prepare visible sprite colour and premultiplied coverage, in memory only.

    Ignore hidden RGB when metering exposure. A bounded hue-preserving lift and
    authored coverage keep translucent artwork faithful as scene material.
    Black detail, clear pixels and the source file are preserved. Discard alpha
    below five percent: residual colour there is almost invisible
    in normal compositing, but feedback and surface normals amplify it.
    """
    import numpy as np
    stride=max(1,max(pixels.shape[:2])//128)
    sample=pixels[::stride,::stride].astype(np.float32)/255.
    luminance=sample[:,:,:3]@np.array([.2126,.7152,.0722],dtype=np.float32)
    # Faint matte can cover most of a cutout. Meter its visible body only.
    visible=(sample[:,:,3]>=ALPHA_EXPOSURE_MIN/255)&(luminance>.02)
    anchor=float(np.percentile(luminance[visible],90)) if visible.any() else 1.
    exposure=max(1.,min(1.8,.72/max(anchor,.001)))
    rgb=pixels[:,:,:3].astype(np.float32)*(exposure/255.)
    rgb/=np.maximum(1.,rgb.max(axis=2,keepdims=True))
    coverage=pixels[:,:,3:4].astype(np.float32)/255.
    coverage=np.where(pixels[:,:,3:4]<=ALPHA_NOISE_MAX,0.,coverage)
    alpha=coverage
    prepared=np.empty_like(pixels)
    prepared[:,:,:3]=np.rint(rgb*alpha*255).astype(np.uint8)
    prepared[:,:,3:4]=np.rint(alpha*255).astype(np.uint8)
    return prepared

def content_identity(path):
    with open(path,'rb') as source:return hashlib.file_digest(source,'sha256').hexdigest()

def image_role(path):
    with warnings.catch_warnings():
        warnings.simplefilter('error',Image.DecompressionBombWarning)
        with Image.open(path) as source:
            if source.width*source.height>16_777_216:raise ValueError('Scene image exceeds 16 megapixels')
            rgba=source.convert('RGBA');rgba.thumbnail((128,128))
            hist=rgba.getchannel('A').histogram();count=sum(hist)
            if sum(hist[ALPHA_NOISE_MAX+1:])==0:raise ValueError('Image is fully transparent')
            # Ignore an isolated transparent pixel or an otherwise opaque alpha channel.
            return 'cutout' if sum(hist[:250])/count>.01 else 'background'
