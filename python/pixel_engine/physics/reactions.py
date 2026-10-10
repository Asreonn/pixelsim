
import random
from pixel_engine.core.grid import Grid
from pixel_engine.materials.registry import materials, ID_AIR, ID_FIRE
from pixel_engine.core.types import MaterialType

def update_reactions(grid: Grid, x: int, y: int, mat_id: int):
    """
    Handles chemical reactions, phase changes, and thermal updates.
    Returns True if the pixel changed state/died, False otherwise.
    """
    material = materials[mat_id]
    temp = grid.get_temperature(x, y)
    
    # 1. Lifetime Check (for Fire, Steam, etc.)
    # We use a randomized decay to avoid all pixels vanishing at once
    if material.lifetime > 0:
        # Simple chance to decay based on lifetime. 
        # A lifetime of 60 means ~1/60 chance to die per frame ideally, 
        # or we could store age. Storing age per pixel requires another matrix.
        # For simplicity/speed, we use random chance.
        if random.random() < (1.0 / material.lifetime):
            if material.low_temp_transition_target != -1:
                 # Revert to condensed form (Steam -> Water)
                 grid.set_material(x, y, material.low_temp_transition_target)
            else:
                 # Vanish (Fire -> Air)
                 grid.set_material(x, y, ID_AIR)
            return True

    # 2. Phase Transitions (Temperature based)
    
    # Heating up (Boiling / Melting)
    if temp > material.boiling_point and material.high_temp_transition_target != -1:
        grid.set_material(x, y, material.high_temp_transition_target)
        # Energy loss due to phase change? Usually temp stays at boiling point
        # grid.set_temperature(x, y, material.boiling_point) 
        return True
    
    # Melting (Solid -> Liquid) - treated same as above if structure allows
    # We used high_temp_transition_target for both melting and boiling in BaseMaterial?
    # Actually BaseMaterial has melting_point, boiling_point.
    # We usually need specific targets for each.
    # In registry, Water has high_temp -> Steam.
    # Sand has melting_point but no target defined in registry snippet? (Comment said TODO)
    # If target is missing, we ignore.
    if temp > material.melting_point and material.high_temp_transition_target != -1:
         grid.set_material(x, y, material.high_temp_transition_target)
         return True

    # Cooling down (Freezing / Condensing)
    # Use low_temp_transition_target
    if material.low_temp_transition_target != -1:
        # Determine threshold: if gas, condense at boiling point. If liquid, freeze at melting point?
        # A simpler approach: check both thresholds against current state
        threshold = material.boiling_point if material.type == MaterialType.GAS else material.melting_point
        
        # Correction: If it is Steam (Gas), it condenses below 100.
        # If it is Water (Liquid), it freezes below 0 (if defined).
        # We rely on configured props.
        if temp < threshold:
             grid.set_material(x, y, material.low_temp_transition_target)
             return True

    # 3. Thermal Interaction (Burning / Heating Neighbors)
    # If this pixel is hot (Fire), heat neighbors.
    # If this pixel is fuel and neighbor is hot, ignite.
    
    if material.temperature > 100 or mat_id == ID_FIRE:
        # Heat neighbors
        heat_transfer = temp * material.conductivity
        # Iterate neighbors
        for dy in [-1, 0, 1]:
            for dx in [-1, 0, 1]:
                if dx == 0 and dy == 0: continue
                nx, ny = x + dx, y + dy
                if grid.is_within_bounds(nx, ny):
                    n_mat_id = grid.get_material(nx, ny)
                    n_mat = materials[n_mat_id]
                    
                    # Transfer heat
                    n_temp = grid.get_temperature(nx, ny)
                    # Simple equalization tendency
                    if temp > n_temp:
                         # Very basic diffusion
                         grid.set_temperature(nx, ny, n_temp + (temp - n_temp) * 0.1 * material.conductivity)

                    # Ignition
                    # Check if neighbor is flammable and hot enough to ignite (e.g. > 100) or we are very hot
                    if n_mat.flammability > 0:
                        # Probability to catch fire based on flammability and heat
                         if random.random() < n_mat.flammability and temp > 300:
                             grid.set_material(nx, ny, ID_FIRE)
                             grid.set_temperature(nx, ny, 800) # Fire temp

    return False
