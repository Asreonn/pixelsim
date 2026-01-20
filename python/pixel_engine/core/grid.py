
import numpy as np
from pixel_engine.config import GRID_WIDTH, GRID_HEIGHT

class Grid:
    def __init__(self):
        self.width = GRID_WIDTH
        self.height = GRID_HEIGHT
        
        # 2D Array for Material IDs (int)
        # 0 = Air (default)
        self.material_matrix = np.zeros((self.height, self.width), dtype=np.int32)
        
        # 2D Array for Temperature (float)
        # Default ambient temp = 20.0
        self.temp_matrix = np.full((self.height, self.width), 20.0, dtype=np.float32)
        
        # 2D Array to track if a pixel has been updated this frame
        # Prevents double-updating a particle moving down
        self.moved_matrix = np.zeros((self.height, self.width), dtype=bool)

    def is_within_bounds(self, x: int, y: int) -> bool:
        """Checks if coordinates are within the grid."""
        return 0 <= x < self.width and 0 <= y < self.height

    def get_material(self, x: int, y: int) -> int:
        """Returns the material ID at (x, y). Returns 0 (Air) if out of bounds."""
        if self.is_within_bounds(x, y):
            return self.material_matrix[y, x]
        return 0 

    def set_material(self, x: int, y: int, material_id: int):
        """Sets the material ID at (x, y)."""
        if self.is_within_bounds(x, y):
            self.material_matrix[y, x] = material_id

    def get_temperature(self, x: int, y: int) -> float:
        if self.is_within_bounds(x, y):
            return self.temp_matrix[y, x]
        return 20.0

    def set_temperature(self, x: int, y: int, temp: float):
        if self.is_within_bounds(x, y):
            self.temp_matrix[y, x] = temp

    def move_pixel(self, x1, y1, x2, y2):
        """Moves pyixel from (x1, y1) to (x2, y2). Overwrites target."""
        if not self.is_within_bounds(x2, y2): return
        
        self.material_matrix[y2, x2] = self.material_matrix[y1, x1]
        self.temp_matrix[y2, x2] = self.temp_matrix[y1, x1]
        self.moved_matrix[y2, x2] = True
        
        # Clear source
        self.material_matrix[y1, x1] = 0 # Air
        self.temp_matrix[y1, x1] = 20.0 # Ambient
        
    def swap_pixels(self, x1, y1, x2, y2):
        """Swaps pixels between (x1, y1) and (x2, y2)."""
        if not self.is_within_bounds(x2, y2): return

        # Swap Material
        m1 = self.material_matrix[y1, x1]
        m2 = self.material_matrix[y2, x2]
        self.material_matrix[y1, x1] = m2
        self.material_matrix[y2, x2] = m1
        
        # Swap Temp
        t1 = self.temp_matrix[y1, x1]
        t2 = self.temp_matrix[y2, x2]
        self.temp_matrix[y1, x1] = t2
        self.temp_matrix[y2, x2] = t1
        
        # Mark both as moved
        self.moved_matrix[y1, x1] = True
        self.moved_matrix[y2, x2] = True

    def clear_moved_matrix(self):
        """Resets the movement tracking matrix for the next frame."""
        self.moved_matrix.fill(False)

    def reset(self):
        """Clears the entire grid."""
        self.material_matrix.fill(0)
        self.temp_matrix.fill(20.0)
        self.clear_moved_matrix()
