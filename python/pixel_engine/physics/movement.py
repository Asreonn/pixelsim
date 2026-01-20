
import random
from pixel_engine.core.grid import Grid
from pixel_engine.materials.registry import materials, ID_AIR
from pixel_engine.core.types import MaterialType

def update_movement(grid: Grid, x: int, y: int, mat_id: int):
    """
    Handles movement physics for a single pixel based on its state (Solid, Liquid, Gas).
    Returns True if the pixel moved, False otherwise.
    """
    material = materials[mat_id]
    
    # Check if already moved this frame
    if grid.moved_matrix[y, x]:
        return False
    
    # Static materials (like bedrock/walls) don't move
    if material.mass == 0 and material.type == MaterialType.SOLID:
        return False

    # Get gravity direction (1 for down, -1 for up) based on gravity_scale
    # Standard gravity is positive (falls down)
    # Anti-gravity (Gas/Fire) is negative (rises)
    dy = 1 if material.gravity_scale >= 0 else -1
    
    # Target Y coordinate
    target_y = y + dy
    
    if not grid.is_within_bounds(x, target_y):
        return False

    # Check the pixel directly in the direction of gravity
    target_mat_id = grid.get_material(x, target_y)
    
    # Move if target is empty (Air)
    if target_mat_id == ID_AIR:
        grid.move_pixel(x, y, x, target_y)
        return True
    
    # Density Check: Can we displace the target?
    # Heavier things displace lighter things (Sand sinks in Water, Smoke rises in Air)
    target_mat = materials[target_mat_id]
    
    # Basic density displacement (falling solid/liquid displacing liquid/gas)
    if material.density > target_mat.density and target_mat.type in (MaterialType.LIQUID, MaterialType.GAS):
        grid.swap_pixels(x, y, x, target_y)
        return True
        
    # Rise displacement (Lighter gas displacing heavier gas/air)
    if material.type == MaterialType.GAS and material.density < target_mat.density:
         grid.swap_pixels(x, y, x, target_y)
         return True

    # -- Dispersion / Sliding Logic --
    
    # If blocked directly, Liquids and Gases (and some powders) try to move sideways
    if material.type in (MaterialType.LIQUID, MaterialType.GAS, MaterialType.PLASMA):
        # Viscosity Check: Higher viscosity = lower chance to flow sideways
        # 0.0 = Flows always (Water), 1.0 = Never flows (Solid-like)
        if material.viscosity > 0 and random.random() < material.viscosity:
             return False

        direction = random.choice([-1, 1])
        target_x = x + direction
        
        # Check diagonal move (flow)
        if grid.is_within_bounds(target_x, target_y):
            diag_mat_id = grid.get_material(target_x, target_y)
            if diag_mat_id == ID_AIR:
                grid.move_pixel(x, y, target_x, target_y)
                return True
            elif material.density > materials[diag_mat_id].density and materials[diag_mat_id].type != MaterialType.SOLID:
                 grid.swap_pixels(x, y, target_x, target_y)
                 return True

        # Check pure horizontal move (spread)
        # Only if diagonal failed or not desired? Liquids spread on flat surfaces.
        if grid.is_within_bounds(target_x, y):
             side_mat_id = grid.get_material(target_x, y)
             if side_mat_id == ID_AIR:
                 grid.move_pixel(x, y, target_x, y)
                 return True

    # Powders (Sand) try to pile up (diag down)
    elif material.type == MaterialType.SOLID and material.friction < 1.0:
        # Randomly try left or right diagonal
        direction = random.choice([-1, 1])
        target_x = x + direction
        
        if grid.is_within_bounds(target_x, target_y):
            diag_mat_id = grid.get_material(target_x, target_y)
            if diag_mat_id == ID_AIR: 
                grid.move_pixel(x, y, target_x, target_y)
                return True
            elif material.density > materials[diag_mat_id].density and materials[diag_mat_id].type == MaterialType.LIQUID:
                grid.swap_pixels(x, y, target_x, target_y)
                return True
                
    return False
