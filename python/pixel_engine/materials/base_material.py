from pixel_engine.core.types import MaterialType

class BaseMaterial:
    def __init__(
        self, 
        id: int, 
        name: str, 
        color: tuple[int, int, int],
        material_type: MaterialType,
        mass: float = 1.0,
        friction: float = 0.5,
        viscosity: float = 0.0,
        gravity_scale: float = 1.0,
        density: float = 1.0,
        temperature: float = 20.0,
        heat_capacity: float = 1.0,
        conductivity: float = 0.5,
        melting_point: float = float('inf'),
        boiling_point: float = float('inf'),
        sublimation_point: float = float('inf'),
        flammability: float = 0.0,
        fuel_value: float = 0.0,
        low_temp_transition_target: int = -1,
        high_temp_transition_target: int = -1,
        lifetime: int = -1
    ):
        
        self.id = id
        self.name = name
        self.color = color

        # Physical properties
        self.type = material_type
        self.mass = mass
        self.friction = friction
        self.viscosity = viscosity
        self.gravity_scale = gravity_scale
        self.density = density

        # Thermal properties
        self.temperature = temperature
        self.heat_capacity = heat_capacity
        self.conductivity = conductivity
        
        # Phase transition properties
        self.melting_point = melting_point
        self.boiling_point = boiling_point
        self.sublimation_point = sublimation_point
        self.low_temp_transition_target = low_temp_transition_target
        self.high_temp_transition_target = high_temp_transition_target
        
        # Chemical / Lifetime properties
        self.flammability = flammability
        self.fuel_value = fuel_value
        self.lifetime = lifetime

