from pixel_engine.materials.base_material import BaseMaterial
from pixel_engine.core.types import MaterialType

ID_AIR = 0
ID_SAND = 1
ID_WATER = 2
ID_MUD = 3
ID_STEAM = 4
ID_FIRE = 5

materials = {}

# Air (Empty / Vacuum)
materials[ID_AIR] = BaseMaterial(
    id=ID_AIR, 
    name="Air", 
    color=(0, 0, 0), 
    material_type=MaterialType.GAS,
    mass=0.0,
    density=0.0,
    gravity_scale=0.0,
    temperature=20.0,
    conductivity=0.05
)

# Sand (Granular solid)
materials[ID_SAND] = BaseMaterial(
    id=ID_SAND, 
    name="Sand", 
    color=(235, 215, 50), 
    material_type=MaterialType.SOLID,
    mass=1.5,
    density=1.5,
    friction=0.8,
    gravity_scale=1.0,
    temperature=20.0,
    melting_point=1700.0,  # Melts into Glass (TODO: Add Glass)
    heat_capacity=0.8
)

# Water (Liquid)
materials[ID_WATER] = BaseMaterial(
    id=ID_WATER, 
    name="Water", 
    color=(30, 144, 255), 
    material_type=MaterialType.LIQUID,
    mass=1.0,
    density=1.0,
    friction=0.1,
    viscosity=0.2,
    gravity_scale=1.0,
    temperature=20.0,
    boiling_point=100.0,
    high_temp_transition_target=ID_STEAM,
    heat_capacity=4.18  # High heat capacity
)

# Mud (Viscous / Wet Solid)
materials[ID_MUD] = BaseMaterial(
    id=ID_MUD, 
    name="Mud", 
    color=(80, 60, 40), 
    material_type=MaterialType.SOLID, 
    mass=1.8,
    density=1.8,
    friction=0.9,
    viscosity=0.8, 
    gravity_scale=1.0,
    temperature=20.0
)

# Steam (Gas)
materials[ID_STEAM] = BaseMaterial(
    id=ID_STEAM, 
    name="Steam", 
    color=(220, 220, 220), 
    material_type=MaterialType.GAS,
    mass=0.5,
    density=0.5,
    gravity_scale=-0.5, 
    temperature=110.0,
    boiling_point=100.0, 
    low_temp_transition_target=ID_WATER,
    lifetime=600,
    heat_capacity=2.0
)

# Fire (Plasma / Reaction)
materials[ID_FIRE] = BaseMaterial(
    id=ID_FIRE, 
    name="Fire", 
    color=(255, 80, 0), 
    material_type=MaterialType.PLASMA,
    mass=0.1,
    density=0.1,
    gravity_scale=-0.8, 
    temperature=800.0,
    flammability=0.0,
    lifetime=45, 
    heat_capacity=1.0
)
