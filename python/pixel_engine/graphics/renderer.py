
import numpy as np
import ctypes
from pixel_engine.graphics.x11 import X11Window
from pixel_engine.core.grid import Grid
from pixel_engine.materials.registry import materials
from pixel_engine.config import GRID_WIDTH, GRID_HEIGHT, pixelSize, screenSize

class Renderer:
    def __init__(self, window: X11Window):
        self.window = window
        self.width = window.width
        self.height = window.height
        
        # Color LUT
        # X11 standard visual is usually BGR or BGRA?
        # Usually TrueColor is B G R A (little endian) -> B=0, G=1, R=2, A=3?
        # Let's try BGRA.
        self.color_lut = np.zeros((256, 4), dtype=np.uint8)
        
        for mat_id, mat in materials.items():
            if mat_id < 256:
                r, g, b = mat.color
                # BGRA (Blue, Green, Red, Alpha/Padding)
                self.color_lut[mat_id] = [b, g, r, 0] # A=255? X11 usually ignores top byte for 24bit depth
    
    def render(self, grid: Grid):
        """
        Updates the X11 window.
        """
        # 1. Map Material Matrix to BGRA Matrix
        # shape: (H, W, 4)
        bgra_matrix = self.color_lut[grid.material_matrix]
        
        # 2. Scale Up?
        # Our window is screenSize (1280x720). Grid is smaller.
        # We must scale.
        # NumPy repeat is fast-ish.
        if pixelSize > 1:
            # Axis 0 (Y) -> repeat rows
            # Axis 1 (X) -> repeat cols
            # shape changes from (H, W, 4) to (H*S, W*S, 4)
            scaled = bgra_matrix.repeat(pixelSize, axis=0).repeat(pixelSize, axis=1)
            
            # 3. Send to Window
            # Flatten to bytes
            self.window.put_buffer(scaled.tobytes())
        else:
            self.window.put_buffer(bgra_matrix.tobytes())
            
        self.window.flush()
