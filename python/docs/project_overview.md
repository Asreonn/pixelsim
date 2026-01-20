# Project Overview

Date: 2026-01-20
Status: Milestone A (Sand simulation with fixed timestep)

## 1. Pixel Data Structure (What's in a cell?)

The simulation uses a **Structure of Arrays (SoA)** memory layout. This means instead of one big `struct` per pixel, we have separate arrays for each property. This improves cache performance and allows SIMD optimizations.

**Every single pixel (cell) in the 512x512 grid has the following properties:**

| Field | Type | Description | Memory Usage |
| :--- | :--- | :--- | :--- |
| `mat` | `uint8_t` | Material ID (0-255). Defines "what" the pixel is. | 256 KB |
| `mat_next` | `uint8_t` | Double buffer for material (next frame state). | 256 KB |
| `flags` | `uint16_t` | State flags (Burning, Wet, Hot, Frozen, etc.). | 512 KB |
| `color_seed` | `uint32_t` | Random seed for visual noise/texture variation. | 1 MB |
| `lifetime` | `uint8_t` | Remaining life for temporary particles (Fire, Smoke). | 256 KB |
| `temp` | `float` | Temperature in Celsius. | 1 MB |
| `temp_next` | `float` | Double buffer for temperature. | 1 MB |
| `energy` | `float` | Thermal energy in Joules (derived from Temp * Mass). | 1 MB |
| `phase_progress`| `float` | 0.0 to 1.0 progress for melting/boiling. | 1 MB |
| `pressure` | `float` | Pressure in Pascals (for explosions/fluids). | 1 MB |
| `oxygen` | `float` | 0.0 to 1.0 (1.0 = 100% O2). Used for fire. | 1 MB |
| `fuel` | `float` | Remaining fuel for combustion. | 1 MB |
| `vel_x`, `vel_y`| `int16_t` | Velocity X/Y in fixed-point (8.8 format). | 1 MB |
| `chunk_active` | `bool` | Optimization flag (is this chunk awake?). | Sparse |

**Total Memory per Pixel:** ~52 bytes (approx 13 MB total for 512x512 grid).

---

## 2. Material Classes (States of Matter)

Physics behavior is determined primarily by the **State** of the material:

### 💨 GAS (e.g., Smoke, Steam, Fire)
*   **Movement**: Flows freely, expands to fill space.
*   **Buoyancy**: Highly affected. Hot gases rise rapidly.
*   **Physics**: Uses `pressure` and density diffusion logic.

### 💧 FLUID (e.g., Water, Oil, Lava, Acid)
*   **Movement**: Flows sideways and downwards. Levels out to form flat surfaces.
*   **Buoyancy**: Liquids stack based on density (Oil floats on Water).
*   **Physics**: Supports viscosity (Lava flows slower than Water) and pressure equalization.

### 🏜️ POWDER (e.g., Sand, Soil, Salt, Ember)
*   **Movement**: Falls down. If blocked, tries to slide diagonally (angle of repose).
*   **Physics**: Can pile up. Friction determines how steep the pile can be.

### 🧱 SOLID (e.g., Stone, Wood, Metal, Glass)
*   **Movement**: None (Static).
*   **Physics**: Blocks movement of other states. Can be heated, melted, or corroded.

---

## 3. Material Encyclopedia

Detailed statistics for all implemented materials.

**Legend:**
*   **Den**: Density ($kg/m^3$)
*   **Melt**: Melting Point (°C)
*   **Boil**: Boiling Point (°C)
*   **Flam**: Flammability (0.0 - 1.0)
*   **Cond**: Thermal Conductivity ($W/m\cdot K$)

### Solids & Powders

| ID | Name | State | Den | Melt | Boil | Flam | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 1 | **Sand** | Powder | 1600 | 1700 | - | 0.0 | Melts into Glass. Heaviest powder. |
| 2 | **Stone** | Solid | 2600 | 1000 | - | 0.0 | Forms barriers. Melts into Lava. |
| 4 | **Wood** | Solid | 600 | - | - | 0.8 | Highly flammable. Burns into Ember + Smoke. |
| 7 | **Soil** | Powder | 1800 | - | - | 0.0 | Generic ground. Can support plant growth (future). |
| 8 | **Ice** | Solid | 917 | 0 | 100 | 0.0 | Melts into Water. Slippery friction. |
| 12 | **Metal** | Solid | 7800 | 1500 | - | 0.0 | Conducts heat very well. Melts into Molten Metal. |
| 16 | **Wet Wood**| Solid | 900 | - | 100 | 0.1 | Hard to burn until water boils off. |
| 17 | **Glass** | Solid | 2500 | 1700 | - | 0.0 | Formed from melted Sand. Transparent. |
| 18 | **Obsidian**| Solid | 2600 | 1200 | - | 0.0 | Formed when Lava touches Water. Hard. |
| 19 | **Coal** | Solid | 1400 | - | - | 0.6 | Burns slowly but very hot (High energy). |
| 20 | **Salt** | Powder | 2170 | 801 | 1465 | 0.0 | Dissolves in water (lowers freezing point). |
| 13 | **Mud** | Powder | 1900 | - | 80 | 0.0 | Wet Soil. Drys into Soil when heated. Viscous. |
| 10 | **Ember** | Powder | 400 | - | - | 0.4 | Hot remains of fire. Can reignite. Glows. |

### Fluids & Liquids

| ID | Name | State | Den | Melt | Boil | Flam | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 3 | **Water** | Fluid | 1000 | 0 | 100 | 0.0 | Generic liquid. Extinguishes fire. High heat cap. |
| 15 | **Oil** | Fluid | 800 | -20 | 300 | 1.0 | Floats on water. Burns violently. |
| 11 | **Acid** | Fluid | 1100 | -20 | 120 | 0.0 | Corrodes solids. Dissolves organic matter. |
| 14 | **Lava** | Fluid | 2500 | 700 | - | 0.0 | Molten rock. Ignites things. Viscous flow. |
| 21 | **Molten Metal**| Fluid | 7000 | 1500 | 2900 | 0.0 | Extremely hot liquid metal. Very heavy. |

### Gases & Plasmas

| ID | Name | State | Den | Life | Rise | Flam | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 0 | **Air** | Gas | 1.2 | - | 0 | 0.0 | Default empty space. Contains Oxygen. |
| 5 | **Fire** | Gas | 0.3 | Short| Fast | - | Plasma state. Generates heat. Consumes O2. |
| 6 | **Smoke** | Gas | 1.1 | Med | Slow | 0.0 | Product of fire. Suffocates fire. |
| 9 | **Steam** | Gas | 0.6 | Med | Fast | 0.0 | Water vapor. Condenses back to water if cooled. |

---

## 4. Physics Interactions (The "Engine Rules")

1.  **Phase Changes (Thermodynamics)**
    *   **Melting**: Solid $\to$ Fluid/Liquid (e.g., Ice $\to$ Water at 0°C).
    *   **Freezing**: Fluid $\to$ Solid (e.g., Lava $\to$ Stone at 700°C).
    *   **Boiling**: Fluid $\to$ Gas (e.g., Water $\to$ Steam at 100°C).
    *   **Condensation**: Gas $\to$ Fluid (e.g., Steam $\to$ Water below 100°C).
    *   *Note: All transitions require **Latent Heat**, meaning a material stays at 0°C for a while while absorbing energy before turning to water.*

2.  **Combustion (Fire Triangle)**
    *   Fire requires: **Fuel** (Wood/Oil) + **Heat** (> Ignition Temp) + **Oxygen** (from Air).
    *   If Oxygen drops to 0 (smothered), fire dies.
    *   Water extinguishes fire by cooling it (removing Heat) and blocking Oxygen.

3.  **Density & Buoyancy**
    *   Heavier things sink, lighter things float.
    *   **Oil (800)** floats on **Water (1000)**.
    *   **Steam (0.6)** rises rapidly through Air.
    *   **Hot Air** rises (Convection). The engine simulates "Hot Air" without a separate material by lowering the effective density of hot Gas cells.

4.  **Corrosion**
    *   **Acid** eats through most materials except Glass and Plastic (not impl).
    *   Reaction generates heat and sometimes gas.
