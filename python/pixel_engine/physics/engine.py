
from pixel_engine.core.grid import Grid
from pixel_engine.physics import movement, reactions
from pixel_engine.materials.registry import ID_AIR

class PhysicsEngine:
    def __init__(self, grid: Grid):
        self.grid = grid

    def update(self):
        """
        Main simulation loop.
        Updates reactions and movement for every pixel.
        """
        self.grid.clear_moved_matrix()
        
        # Iterate over the grid
        # We can optimize by tracking "active chunks" but for now we iterate all.
        # Alternating traversal order (left-to-right / right-to-left) can reduce move bias,
        # but vertical order is handled by moved_matrix checks.
        
        height = self.grid.height
        width = self.grid.width
        
        # Process Bottom-Up (Standard for falling sand to feel responsive)
        # Even with moved_matrix, bottom-up is usually preferred for stacking logic optimization.
        for y in range(height - 1, -1, -1):
            for x in range(width):
                
                # Use direct access helper or the grid methods
                # Calling get_material 50k times is slow, but safe.
                mat_id = self.grid.get_material(x, y)
                
                if mat_id == ID_AIR:
                    continue
                
                # 1. Reactions (Phase, Burning, Life)
                # If reaction destroys/changes the pixel, we might skip movement?
                # If it changes to Gas, it might want to move UP this same frame.
                # If it dies (Air), we stop.
                if reactions.update_reactions(self.grid, x, y, mat_id):
                    # Material might have changed ID (Warer -> Steam) or become Air.
                    # Update local var
                    mat_id = self.grid.get_material(x, y)
                    if mat_id == ID_AIR:
                        continue
                        
                # 2. Movement
                movement.update_movement(self.grid, x, y, mat_id)

    def draw_debug(self):
        # Optional debug print?
        pass
