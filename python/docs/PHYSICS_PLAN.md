# Pixel Simulation - Kapsamli Fizik Sistemi Plani

## Genel Bakis

Bu dokuman, piksel simulasyonuna eklenecek muhendislik seviyesinde fizik sistemlerini detayli olarak aciklar.

### Temel Kararlar

| Karar | Secim | Aciklama |
|-------|-------|----------|
| Sicaklik Birimi | Celsius (C) | Formuller icinde Kelvin'e donusturulur |
| Oksijen Sistemi | Hucre Bazli | Her hucrede ayri O2 seviyesi |
| MAT_EMPTY | Kaldirildi | Yerine MAT_AIR kullanilacak |
| Oncelik | Gercekcilik | Performanstan once fizik dogrulugu |

---

## Faz 0: Temel Altyapi

### 0.1 Fizik Sabitleri

```c
// Evrensel sabitler
#define STEFAN_BOLTZMANN    5.67e-8f    // W/(m^2*K^4)
#define GAS_CONSTANT        8.314f      // J/(mol*K)
#define GRAVITY_MPS2        9.81f       // m/s^2
#define ATM_PRESSURE        101325.0f   // Pa

// Simulasyon olcekleme
#define CELL_SIZE_M         0.01f       // 1 cm per cell
#define CELL_VOLUME_M3      1.0e-6f     // 1 cm^3
#define CELL_AREA_M2        1.0e-4f     // 1 cm^2
#define TIME_SCALE          0.016f      // ~60 FPS, 16ms per tick

// Sicaklik donusumleri
#define C_TO_K(c)           ((c) + 273.15f)
#define K_TO_C(k)           ((k) - 273.15f)
```

### 0.2 Yeni Grid Dizileri

```c
// World struct'a eklenecek
float* oxygen;          // Her hucrede O2 miktari (0-1, 0.21 = atmosferik)
float* pressure;        // Basinc (Pa)
float* energy;          // Termal enerji (J)
float* phase_progress;  // Faz degisimi ilerlemesi (0-1)
float* effective_density; // Sicakliga bagli efektif yogunluk
```

---

## Faz 1: Malzeme Sistemi Yeniden Tasarimi

### 1.1 Yeni MaterialProps Alanlari

```c
typedef struct {
    // === MEVCUT ALANLAR (korunacak) ===
    MaterialID id;
    const char* name;
    MaterialState state;
    Color base_color;
    uint8_t color_variation;
    float density;              // kg/m^3
    float mass;                 // kg (hesaplanmis)
    float friction;
    float restitution;
    float cohesion;
    float viscosity;            // Pa*s
    float surface_tension;      // N/m
    float porosity;
    float permeability;
    float bulk_modulus;
    float thermal_expansion;    // 1/K
    float gravity_scale;
    float drag_coeff;
    float terminal_velocity;
    float flow_rate;
    float settle_probability;
    float slide_bias;
    float conductivity;         // W/(m*K)
    float heat_capacity;        // J/(kg*K)
    float ignition_temp;        // C
    float burn_rate;
    float smoke_rate;
    float melting_temp;         // C
    float boiling_temp;         // C
    
    // === YENI ALANLAR ===
    
    // Gizli isi (faz degisimleri)
    float latent_heat_fusion;       // J/kg - erime gizli isisi
    float latent_heat_vaporization; // J/kg - buharlasma gizli isisi
    
    // Isima ve emisyon
    float emissivity;               // 0-1 - Stefan-Boltzmann icin
    
    // Yanma kimyasi
    float heat_of_combustion;       // J/kg - yanma isisi
    float activation_energy;        // J/mol - Arrhenius icin
    float oxygen_consumption;       // mol O2 / mol yakit
    float stoichiometric_ratio;     // Hava/yakit orani
    
    // Gaz ozellikleri
    float molar_mass;               // kg/mol
    float specific_gas_constant;    // J/(kg*K) = R/M
    float adiabatic_index;          // gamma = Cp/Cv
    
    // Yuzey etkilesimleri
    float cohesion_strength;        // N/m - ayni malzeme cekimi
    float adhesion_water;           // 0-1 - suya yapisma
    float adhesion_oil;             // 0-1 - yaga yapisma
    
    // Kimyasal ozellikler
    float solubility_water;         // kg/L - suda cozunurluk
    float corrosion_resistance;     // 0-1 - aside direnme
    float reactivity;               // 0-1 - genel reaktivite
    
    // Elektrik (gelecek icin)
    float electrical_conductivity;  // S/m
    
    // Onbelleklenmis degerler
    float thermal_mass;             // m * c (hesaplanmis)
    float inv_thermal_mass;         // 1 / (m * c)
    
} MaterialProps;
```

### 1.2 Malzeme ID Guncellemesi

```c
// types.h - Yeni malzeme tanimlari
typedef uint8_t MaterialID;

#define MAT_AIR         0   // Eski EMPTY yerine
#define MAT_SAND        1
#define MAT_STONE       2
#define MAT_WATER       3
#define MAT_WOOD        4
#define MAT_FIRE        5
#define MAT_SMOKE       6
#define MAT_SOIL        7
#define MAT_ICE         8
#define MAT_STEAM       9
#define MAT_EMBER       10  // Eski ASH yerine
#define MAT_ACID        11
#define MAT_METAL       12
#define MAT_MUD         13
#define MAT_LAVA        14
#define MAT_OIL         15
#define MAT_WET_WOOD    16
#define MAT_GLASS       17  // YEN
#define MAT_OBSIDIAN    18  // YENI
#define MAT_COAL        19  // YENI
#define MAT_SALT        20  // YENI
#define MAT_MOLTEN_METAL 21 // YENI - eriyen metal

#define MAT_COUNT       22
```

### 1.3 Detayli Malzeme Tanimlari

#### MAT_AIR (Hava)
```c
{
    .id = MAT_AIR,
    .name = "Air",
    .state = STATE_GAS,
    .base_color = {0, 0, 0, 0},         // Saydam
    .density = 1.225f,                   // kg/m^3 (20C, 1 atm)
    .viscosity = 0.0000181f,             // Pa*s
    .thermal_expansion = 0.00341f,       // 1/K (ideal gaz: 1/T)
    .gravity_scale = 0.0f,               // Statik (konveksiyon ayri hesaplanir)
    .conductivity = 0.026f,              // W/(m*K)
    .heat_capacity = 1005.0f,            // J/(kg*K)
    .molar_mass = 0.029f,                // kg/mol
    .specific_gas_constant = 287.0f,     // J/(kg*K)
    .adiabatic_index = 1.4f,             // gamma
    .emissivity = 0.0f,                  // Hava isima yapmaz
}
```

#### MAT_WATER (Su)
```c
{
    .id = MAT_WATER,
    .name = "Water",
    .state = STATE_FLUID,
    .base_color = {50, 100, 200, 180},
    .density = 1000.0f,                  // kg/m^3
    .viscosity = 0.001f,                 // Pa*s (20C)
    .surface_tension = 0.0728f,          // N/m
    .thermal_expansion = 0.00021f,       // 1/K
    .gravity_scale = 1.0f,
    .flow_rate = 0.7f,
    .conductivity = 0.6f,                // W/(m*K)
    .heat_capacity = 4186.0f,            // J/(kg*K) - cok yuksek!
    .melting_temp = 0.0f,                // C
    .boiling_temp = 100.0f,              // C
    .latent_heat_fusion = 334000.0f,     // J/kg
    .latent_heat_vaporization = 2260000.0f, // J/kg
    .cohesion_strength = 0.0728f,        // = surface tension
    .emissivity = 0.95f,
}
```

#### MAT_WOOD (Odun)
```c
{
    .id = MAT_WOOD,
    .name = "Wood",
    .state = STATE_SOLID,
    .base_color = {139, 90, 43, 255},
    .density = 600.0f,                   // kg/m^3 (cam)
    .conductivity = 0.12f,               // W/(m*K) - kotu iletken
    .heat_capacity = 1700.0f,            // J/(kg*K)
    .ignition_temp = 300.0f,             // C
    .heat_of_combustion = 15000000.0f,   // J/kg (15 MJ/kg)
    .activation_energy = 125000.0f,      // J/mol
    .oxygen_consumption = 1.0f,          // mol O2 / mol C
    .stoichiometric_ratio = 6.0f,        // kg hava / kg odun
    .emissivity = 0.9f,
    .corrosion_resistance = 0.3f,        // Aside zayif
}
```

#### MAT_FIRE (Ates)
```c
{
    .id = MAT_FIRE,
    .name = "Fire",
    .state = STATE_GAS,                  // Sicak gaz
    .base_color = {255, 150, 50, 200},
    .density = 0.3f,                     // kg/m^3 (sicak gaz)
    .gravity_scale = -0.8f,              // Yukselir
    .conductivity = 0.05f,               // Dusuk iletim
    .heat_capacity = 1000.0f,
    .emissivity = 0.95f,                 // Cok isima yapar
    .oxygen_consumption = 0.1f,          // Her tick O2 tuketir
}
```

#### MAT_SMOKE (Duman)
```c
{
    .id = MAT_SMOKE,
    .name = "Smoke",
    .state = STATE_GAS,
    .base_color = {60, 60, 60, 150},
    .density = 1.1f,                     // Havadan biraz agir
    .gravity_scale = -0.2f,              // Yavas yukselir
    .conductivity = 0.02f,
    .heat_capacity = 1000.0f,
    .emissivity = 0.1f,
    .molar_mass = 0.044f,                // CO2 yakini
}
```

#### MAT_EMBER (Kor)
```c
{
    .id = MAT_EMBER,
    .name = "Ember",
    .state = STATE_SOLID,                // Kati ama kivlicim sacabilir
    .base_color = {180, 80, 20, 255},
    .density = 400.0f,                   // Hafif komur
    .conductivity = 0.1f,
    .heat_capacity = 800.0f,
    .ignition_temp = 250.0f,             // Dusuk - tekrar yanabilir
    .heat_of_combustion = 5000000.0f,    // Kalan enerji
    .emissivity = 0.85f,                 // Isima yapar
    .oxygen_consumption = 0.05f,         // Yavas tuketim
}
```

#### MAT_GLASS (Cam)
```c
{
    .id = MAT_GLASS,
    .name = "Glass",
    .state = STATE_SOLID,
    .base_color = {200, 220, 255, 100},  // Saydam
    .density = 2500.0f,
    .conductivity = 1.0f,
    .heat_capacity = 840.0f,
    .melting_temp = 1700.0f,             // C
    .emissivity = 0.92f,
    .corrosion_resistance = 0.7f,        // Aside dayanikli
}
```

#### MAT_OBSIDIAN (Obsidyen)
```c
{
    .id = MAT_OBSIDIAN,
    .name = "Obsidian",
    .state = STATE_SOLID,
    .base_color = {20, 15, 30, 255},     // Koyu mor-siyah
    .density = 2600.0f,
    .conductivity = 1.5f,
    .heat_capacity = 900.0f,
    .melting_temp = 1200.0f,
    .emissivity = 0.95f,
    .corrosion_resistance = 0.9f,        // Cok dayanikli
}
```

#### MAT_COAL (Komur)
```c
{
    .id = MAT_COAL,
    .name = "Coal",
    .state = STATE_SOLID,
    .base_color = {30, 30, 35, 255},
    .density = 1400.0f,
    .conductivity = 0.2f,
    .heat_capacity = 1260.0f,
    .ignition_temp = 427.0f,             // C - odundan yuksek
    .heat_of_combustion = 32000000.0f,   // J/kg (32 MJ/kg)
    .activation_energy = 150000.0f,
    .oxygen_consumption = 2.67f,         // C + O2 -> CO2
    .stoichiometric_ratio = 11.5f,
    .emissivity = 0.9f,
}
```

#### MAT_SALT (Tuz)
```c
{
    .id = MAT_SALT,
    .name = "Salt",
    .state = STATE_POWDER,
    .base_color = {240, 240, 245, 255},
    .density = 2170.0f,
    .conductivity = 0.5f,
    .heat_capacity = 880.0f,
    .melting_temp = 801.0f,
    .solubility_water = 0.36f,           // kg/L
    .gravity_scale = 1.0f,
}
```

#### MAT_MOLTEN_METAL (Eriyen Metal)
```c
{
    .id = MAT_MOLTEN_METAL,
    .name = "Molten Metal",
    .state = STATE_FLUID,
    .base_color = {255, 200, 100, 255},  // Parlak turuncu
    .density = 7000.0f,                  // Sivilasmis celik
    .viscosity = 0.006f,                 // Pa*s
    .conductivity = 40.0f,               // Hala iyi iletken
    .heat_capacity = 600.0f,
    .melting_temp = 1500.0f,             // Alti donma
    .emissivity = 0.4f,
    .flow_rate = 0.3f,                   // Yavas akar
}
```

---

## Faz 2: Enerji Tabanli Termal Sistem

### 2.1 Temel Formul

```
Enerji denklemi:
E = m * c * T

Isı transferi:
Q = k * A * (T2 - T1) * dt / dx

Sicaklik degisimi:
dT = Q / (m * c)
```

### 2.2 Enerji Korunumu

```c
// Her tick basinda:
float total_energy_start = calculate_total_energy(world);

// ... simulasyon ...

// Her tick sonunda:
float total_energy_end = calculate_total_energy(world);
float energy_error = total_energy_end - total_energy_start;

// Hata cok buyukse uyari ver
if (fabs(energy_error) > ENERGY_TOLERANCE) {
    // Normalize et veya log'la
}
```

### 2.3 Gizli Isi Sistemi

```c
void thermal_apply_latent_heat(World* world, int x, int y) {
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    const MaterialProps* props = material_get(mat);
    float temp = world->temp[idx];
    float energy = world->energy[idx];
    
    // Erime kontrolu
    if (mat == MAT_ICE && temp >= 0.0f) {
        float required_energy = props->mass * props->latent_heat_fusion;
        
        world->phase_progress[idx] += energy / required_energy;
        
        if (world->phase_progress[idx] >= 1.0f) {
            world_set_mat(world, x, y, MAT_WATER);
            world->phase_progress[idx] = 0.0f;
            world->temp[idx] = 0.0f;  // Sicaklik erime noktasinda kalir
        } else {
            world->temp[idx] = 0.0f;  // Erime surerken sicaklik sabit
        }
    }
    
    // Buharlasma kontrolu
    if (mat == MAT_WATER && temp >= 100.0f) {
        float required_energy = props->mass * props->latent_heat_vaporization;
        
        world->phase_progress[idx] += energy / required_energy;
        
        if (world->phase_progress[idx] >= 1.0f) {
            world_set_mat(world, x, y, MAT_STEAM);
            world->phase_progress[idx] = 0.0f;
            world->temp[idx] = 100.0f;
        } else {
            world->temp[idx] = 100.0f;  // Kaynama surerken sicaklik sabit
        }
    }
}
```

### 2.4 Fourier Isi Iletimi

```c
void thermal_conduction(World* world, int x, int y) {
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    const MaterialProps* props = material_get(mat);
    float temp = world->temp[idx];
    float k1 = props->conductivity;
    
    float heat_in = 0.0f;
    
    for (int i = 0; i < 4; i++) {
        int nx = x + NEIGHBOR4_DX[i];
        int ny = y + NEIGHBOR4_DY[i];
        if (!IN_BOUNDS(nx, ny)) continue;
        
        int nidx = IDX(nx, ny);
        MaterialID nmat = world->mat[nidx];
        const MaterialProps* nprops = material_get(nmat);
        float ntemp = world->temp[nidx];
        float k2 = nprops->conductivity;
        
        // Harmonik ortalama ile efektif iletkenlik
        float k_eff = (k1 > 0 && k2 > 0) ? (2.0f * k1 * k2) / (k1 + k2) : 0.0f;
        
        // Fourier yasasi: Q = k * A * dT * dt / dx
        float dT = ntemp - temp;
        float Q = k_eff * CELL_AREA_M2 * dT * TIME_SCALE / CELL_SIZE_M;
        
        heat_in += Q;
    }
    
    // Enerji ve sicaklik guncelle
    world->energy[idx] += heat_in;
    
    // Yeni sicaklik (E = m*c*T => T = E/(m*c))
    if (props->thermal_mass > 0) {
        world->temp_next[idx] = world->energy[idx] * props->inv_thermal_mass;
    }
}
```

### 2.5 Stefan-Boltzmann Radyasyonu

```c
void thermal_radiation(World* world, int x, int y) {
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    const MaterialProps* props = material_get(mat);
    
    if (props->emissivity <= 0.01f) return;  // Isima yapmiyor
    
    float temp_K = C_TO_K(world->temp[idx]);
    float temp_amb_K = C_TO_K(AMBIENT_TEMP);
    
    // Stefan-Boltzmann: P = e * s * A * (T^4 - T_amb^4)
    float T4 = temp_K * temp_K * temp_K * temp_K;
    float Tamb4 = temp_amb_K * temp_amb_K * temp_amb_K * temp_amb_K;
    
    float P_rad = props->emissivity * STEFAN_BOLTZMANN * CELL_AREA_M2 * (T4 - Tamb4);
    
    // Olcekleme (T^4 cok buyuk sayilar uretir)
    P_rad *= RADIATION_SCALE;  // Ornegin 1e-6
    
    // Enerji kaybi
    world->energy[idx] -= P_rad * TIME_SCALE;
    
    // Yakin hucrelere isima transferi (opsiyonel)
    // Bu daha karmasik ve pahali
}
```

---

## Faz 3: Kaldirma Kuvveti ve Konveksiyon

### 3.1 Archimedes Prensibi

```c
float buoyancy_calculate_force(World* world, int x, int y) {
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    const MaterialProps* props = material_get(mat);
    
    // Efektif yogunluk (sicakliga bagli)
    float temp = world->temp[idx];
    float beta = props->thermal_expansion;
    float rho_eff = props->density / (1.0f + beta * (temp - AMBIENT_TEMP));
    
    // Cevre yogunlugunu hesapla (4 komsu ortalamasi)
    float rho_fluid = 0.0f;
    int count = 0;
    
    for (int i = 0; i < 4; i++) {
        int nx = x + NEIGHBOR4_DX[i];
        int ny = y + NEIGHBOR4_DY[i];
        if (!IN_BOUNDS(nx, ny)) continue;
        
        int nidx = IDX(nx, ny);
        rho_fluid += world->effective_density[nidx];
        count++;
    }
    
    if (count > 0) rho_fluid /= count;
    else rho_fluid = AIR_DENSITY;
    
    // Kaldirma kuvveti: F = (rho_fluid - rho_particle) * V * g
    float F_buoyancy = (rho_fluid - rho_eff) * CELL_VOLUME_M3 * GRAVITY_MPS2;
    
    // Ivmeye cevir: a = F / m
    float accel = F_buoyancy / (props->mass);
    
    return accel;  // Pozitif = yukari, negatif = asagi
}
```

### 3.2 Konveksiyon Hareketi

```c
void buoyancy_update_cell(Simulation* sim, World* world, int x, int y) {
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    MaterialState state = material_state(mat);
    
    // Sadece gaz ve sivilar icin
    if (state != STATE_GAS && state != STATE_FLUID) return;
    
    float buoyancy_accel = buoyancy_calculate_force(world, x, y);
    
    // Hiz guncelle
    world->vel_y[idx] += FIXED_FROM_FLOAT(buoyancy_accel * TIME_SCALE);
    
    // Yukari hareket (kaldirma > 0)
    if (buoyancy_accel > BUOYANCY_THRESHOLD) {
        if (y > 0) {
            int above_idx = IDX(x, y - 1);
            MaterialID above_mat = world->mat[above_idx];
            float above_rho = world->effective_density[above_idx];
            float my_rho = world->effective_density[idx];
            
            // Daha agir madde ile yer degistir
            if (my_rho < above_rho) {
                world_swap_cells(world, x, y, x, y - 1);
            }
        }
    }
    // Asagi hareket (kaldirma < 0)
    else if (buoyancy_accel < -BUOYANCY_THRESHOLD) {
        if (y < GRID_HEIGHT - 1) {
            int below_idx = IDX(x, y + 1);
            float below_rho = world->effective_density[below_idx];
            float my_rho = world->effective_density[idx];
            
            // Daha hafif madde ile yer degistir
            if (my_rho > below_rho) {
                world_swap_cells(world, x, y, x, y + 1);
            }
        }
    }
}
```

### 3.3 Efektif Yogunluk Guncelleme

```c
void buoyancy_update_density(World* world) {
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            int idx = IDX(x, y);
            MaterialID mat = world->mat[idx];
            const MaterialProps* props = material_get(mat);
            
            float temp = world->temp[idx];
            float beta = props->thermal_expansion;
            
            // rho_eff = rho_0 / (1 + beta * dT)
            float dT = temp - AMBIENT_TEMP;
            world->effective_density[idx] = props->density / (1.0f + beta * dT);
        }
    }
}
```

---

## Faz 4: Basinc Sistemi

### 4.1 Ideal Gaz Yasasi

```c
void pressure_calculate(World* world, int x, int y) {
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    const MaterialProps* props = material_get(mat);
    
    if (material_state(mat) != STATE_GAS) {
        world->pressure[idx] = ATM_PRESSURE;  // Kati/sivi icin atmosferik
        return;
    }
    
    // PV = nRT => P = (n/V) * R * T = (rho/M) * R * T
    float temp_K = C_TO_K(world->temp[idx]);
    float rho = world->effective_density[idx];
    float M = props->molar_mass;
    
    if (M > 0) {
        world->pressure[idx] = (rho / M) * GAS_CONSTANT * temp_K;
    } else {
        world->pressure[idx] = ATM_PRESSURE;
    }
}
```

### 4.2 Basinc Esitlemesi

```c
void pressure_equalize(Simulation* sim, World* world, int x, int y) {
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    
    if (material_state(mat) != STATE_GAS) return;
    
    float my_pressure = world->pressure[idx];
    
    // En dusuk basincli komsunu bul
    int best_dx = 0, best_dy = 0;
    float min_pressure = my_pressure;
    
    for (int i = 0; i < 4; i++) {
        int nx = x + NEIGHBOR4_DX[i];
        int ny = y + NEIGHBOR4_DY[i];
        if (!IN_BOUNDS(nx, ny)) continue;
        
        int nidx = IDX(nx, ny);
        MaterialID nmat = world->mat[nidx];
        
        if (material_state(nmat) == STATE_GAS) {
            float npress = world->pressure[nidx];
            if (npress < min_pressure - PRESSURE_THRESHOLD) {
                min_pressure = npress;
                best_dx = nx - x;
                best_dy = ny - y;
            }
        }
    }
    
    // Basinc farkina gore hareket
    if (best_dx != 0 || best_dy != 0) {
        float pressure_force = (my_pressure - min_pressure) * PRESSURE_MOVE_FACTOR;
        
        if (simulation_randf(sim) < pressure_force) {
            world_swap_cells(world, x, y, x + best_dx, y + best_dy);
        }
    }
}
```

---

## Faz 5: Yuzey Gerilimi

### 5.1 Kohezyon Kuvveti

```c
float surface_tension_cohesion(World* world, int x, int y) {
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    const MaterialProps* props = material_get(mat);
    
    if (props->cohesion_strength <= 0) return 0.0f;
    
    // Ayni turden komsu say
    int same_neighbors = 0;
    int total_neighbors = 0;
    
    for (int i = 0; i < 8; i++) {
        int nx = x + NEIGHBOR8_DX[i];
        int ny = y + NEIGHBOR8_DY[i];
        if (!IN_BOUNDS(nx, ny)) continue;
        
        total_neighbors++;
        if (world->mat[IDX(nx, ny)] == mat) {
            same_neighbors++;
        }
    }
    
    // Kohezyon faktoru: daha az komsu = daha fazla yuzey = daha fazla gerilim
    float surface_ratio = 1.0f - (float)same_neighbors / (float)total_neighbors;
    
    return props->cohesion_strength * surface_ratio;
}
```

### 5.2 Damlacik Olusumu

```c
bool surface_tension_prevent_split(World* world, int x, int y, int new_x, int new_y) {
    MaterialID mat = world_get_mat(world, x, y);
    const MaterialProps* props = material_get(mat);
    
    if (props->surface_tension <= 0) return false;
    
    // Mevcut konumda komsu sayisi
    int current_neighbors = count_same_neighbors(world, x, y, mat);
    
    // Yeni konumda komsu sayisi
    int new_neighbors = count_same_neighbors(world, new_x, new_y, mat);
    
    // Eger izole kalacaksa hareketi engelle
    if (current_neighbors > 2 && new_neighbors == 0) {
        // Yuzey gerilimi kuvvetine gore engelleme olasiligi
        float prevent_chance = props->surface_tension * 10.0f;  // Olcekleme
        return true;  // veya random check
    }
    
    return false;
}
```

---

## Faz 6: Gelismis Yanma Sistemi

### 6.1 Arrhenius Kinetic

```c
float combustion_calculate_rate(World* world, int x, int y) {
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    const MaterialProps* props = material_get(mat);
    
    if (props->heat_of_combustion <= 0) return 0.0f;  // Yanmaz
    
    float temp = world->temp[idx];
    float temp_K = C_TO_K(temp);
    float oxygen = world->oxygen[idx];
    
    // Tusmasma sicakligi kontrolu
    if (temp < props->ignition_temp) return 0.0f;
    
    // Oksijen kontrolu
    if (oxygen < OXYGEN_MIN_FOR_COMBUSTION) return 0.0f;
    
    // Arrhenius: k = A * exp(-Ea / RT)
    float Ea = props->activation_energy;
    float exponent = -Ea / (GAS_CONSTANT * temp_K);
    
    // exp underflow korumasi
    if (exponent < -50.0f) return 0.0f;
    
    float rate = ARRHENIUS_A * expf(exponent);
    
    // Oksijen ile orantili
    rate *= oxygen / 0.21f;  // Normalize (0.21 = atmosferik)
    
    return rate;
}
```

### 6.2 Yanma Islemesi

```c
void combustion_update_cell(Simulation* sim, World* world, int x, int y) {
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    const MaterialProps* props = material_get(mat);
    
    float burn_rate = combustion_calculate_rate(world, x, y);
    if (burn_rate <= 0.0f) return;
    
    // Yakit tuketimi
    float fuel_consumed = burn_rate * props->mass * TIME_SCALE;
    world->fuel[idx] -= fuel_consumed;
    
    // Isi uretimi
    float heat_produced = fuel_consumed * props->heat_of_combustion;
    world->energy[idx] += heat_produced;
    
    // Oksijen tuketimi
    float oxygen_consumed = props->oxygen_consumption * burn_rate * TIME_SCALE;
    world->oxygen[idx] -= oxygen_consumed;
    world->oxygen[idx] = MAX(0.0f, world->oxygen[idx]);
    
    // Yanma urunleri
    if (world->fuel[idx] <= 0.0f) {
        // Yakit bitti - kor veya kul olustur
        if (mat == MAT_WOOD || mat == MAT_COAL) {
            world_set_mat(world, x, y, MAT_EMBER);
            world->temp[idx] = 500.0f;  // Sicak kor
        }
    }
    
    // Duman uretimi
    if (simulation_randf(sim) < props->smoke_rate * burn_rate) {
        // Yukari bos hucre ara
        if (y > 0 && world->mat[IDX(x, y-1)] == MAT_AIR) {
            world_set_mat(world, x, y - 1, MAT_SMOKE);
            world->temp[IDX(x, y-1)] = world->temp[idx] * 0.8f;
        }
    }
}
```

### 6.3 Oksijen Difuzyonu

```c
void combustion_oxygen_diffuse(World* world) {
    static float oxygen_next[GRID_SIZE];
    
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            int idx = IDX(x, y);
            MaterialID mat = world->mat[idx];
            
            // Sadece gaz hucrelerinde oksijen var
            if (material_state(mat) != STATE_GAS) {
                oxygen_next[idx] = 0.0f;
                continue;
            }
            
            float my_oxygen = world->oxygen[idx];
            float oxygen_in = 0.0f;
            int count = 0;
            
            for (int i = 0; i < 4; i++) {
                int nx = x + NEIGHBOR4_DX[i];
                int ny = y + NEIGHBOR4_DY[i];
                if (!IN_BOUNDS(nx, ny)) continue;
                
                int nidx = IDX(nx, ny);
                if (material_state(world->mat[nidx]) == STATE_GAS) {
                    oxygen_in += (world->oxygen[nidx] - my_oxygen);
                    count++;
                }
            }
            
            if (count > 0) {
                oxygen_next[idx] = my_oxygen + oxygen_in * OXYGEN_DIFFUSION_RATE / count;
            } else {
                oxygen_next[idx] = my_oxygen;
            }
            
            // Atmosferik seviyeye yavas donus (acik sistemde)
            oxygen_next[idx] += (0.21f - oxygen_next[idx]) * OXYGEN_REPLENISH_RATE;
            
            oxygen_next[idx] = CLAMP(oxygen_next[idx], 0.0f, 1.0f);
        }
    }
    
    // Swap buffers
    memcpy(world->oxygen, oxygen_next, sizeof(oxygen_next));
}
```

### 6.4 Alev Renkleri

```c
Color fire_color_from_temperature(float temp_C) {
    float temp_K = C_TO_K(temp_C);
    
    // Blackbody radiation approximation
    if (temp_K < 800.0f) {
        return (Color){80, 20, 10, 200};      // Koyu kirmizi
    } else if (temp_K < 1000.0f) {
        float t = (temp_K - 800.0f) / 200.0f;
        return (Color){
            80 + (uint8_t)(175 * t),           // 80 -> 255
            20 + (uint8_t)(30 * t),            // 20 -> 50
            10,
            200
        };
    } else if (temp_K < 1200.0f) {
        float t = (temp_K - 1000.0f) / 200.0f;
        return (Color){
            255,
            50 + (uint8_t)(100 * t),           // 50 -> 150
            10 + (uint8_t)(40 * t),            // 10 -> 50
            220
        };
    } else if (temp_K < 1500.0f) {
        float t = (temp_K - 1200.0f) / 300.0f;
        return (Color){
            255,
            150 + (uint8_t)(105 * t),          // 150 -> 255
            50 + (uint8_t)(150 * t),           // 50 -> 200
            240
        };
    } else {
        // Beyaz-mavi
        return (Color){255, 255, 230, 255};
    }
}
```

---

## Faz 7: Malzeme Etkilesimleri

### 7.1 Reaksiyon Matrisi

```c
typedef struct {
    MaterialID mat1;
    MaterialID mat2;
    MaterialID result1;      // mat1 ne olur
    MaterialID result2;      // mat2 ne olur
    MaterialID byproduct;    // Yan urun (ust hucreye)
    float probability;       // Reaksiyon olasiligi
    float temp_threshold;    // Min sicaklik (C)
    float energy_release;    // Aciga cikan isi (J)
} Reaction;

static const Reaction REACTIONS[] = {
    // Su + Lav = Buhar + Obsidyen
    {MAT_WATER, MAT_LAVA, MAT_STEAM, MAT_OBSIDIAN, MAT_AIR, 0.9f, 0.0f, 50000.0f},
    
    // Su + Sicak Metal = Buhar
    {MAT_WATER, MAT_METAL, MAT_STEAM, MAT_METAL, MAT_AIR, 0.3f, 100.0f, 10000.0f},
    
    // Kum + Yuksek Isi = Cam
    {MAT_SAND, MAT_FIRE, MAT_GLASS, MAT_FIRE, MAT_AIR, 0.01f, 1700.0f, -50000.0f},
    
    // Buz + Tuz = Tuzlu Su (erime noktasi dusuk)
    {MAT_ICE, MAT_SALT, MAT_WATER, MAT_AIR, MAT_AIR, 0.5f, -21.0f, -5000.0f},
    
    // Asit + Metal = Bosluk + Duman
    {MAT_ACID, MAT_METAL, MAT_AIR, MAT_AIR, MAT_SMOKE, 0.05f, 0.0f, 20000.0f},
    
    // ... daha fazla reaksiyon
};
```

### 7.2 Reaksiyon Isleme

```c
void reactions_check_cell(Simulation* sim, World* world, int x, int y) {
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    float temp = world->temp[idx];
    
    for (int i = 0; i < 4; i++) {
        int nx = x + NEIGHBOR4_DX[i];
        int ny = y + NEIGHBOR4_DY[i];
        if (!IN_BOUNDS(nx, ny)) continue;
        
        int nidx = IDX(nx, ny);
        MaterialID nmat = world->mat[nidx];
        
        // Reaksiyon tablosunda ara
        for (int r = 0; r < REACTION_COUNT; r++) {
            const Reaction* rx = &REACTIONS[r];
            
            bool match1 = (mat == rx->mat1 && nmat == rx->mat2);
            bool match2 = (mat == rx->mat2 && nmat == rx->mat1);
            
            if (!match1 && !match2) continue;
            
            // Sicaklik kontrolu
            if (temp < rx->temp_threshold) continue;
            
            // Olasilik kontrolu
            if (simulation_randf(sim) > rx->probability) continue;
            
            // Reaksiyonu uygula
            if (match1) {
                world_set_mat(world, x, y, rx->result1);
                world_set_mat(world, nx, ny, rx->result2);
            } else {
                world_set_mat(world, x, y, rx->result2);
                world_set_mat(world, nx, ny, rx->result1);
            }
            
            // Enerji aciga cikart
            world->energy[idx] += rx->energy_release * 0.5f;
            world->energy[nidx] += rx->energy_release * 0.5f;
            
            // Yan urun
            if (rx->byproduct != MAT_AIR && y > 0) {
                if (world->mat[IDX(x, y-1)] == MAT_AIR) {
                    world_set_mat(world, x, y - 1, rx->byproduct);
                }
            }
            
            return;  // Bir reaksiyon yeterli
        }
    }
}
```

---

## Faz 8: Simulasyon Pipeline

### 8.1 Guncellenmis Tick Sirasi

```c
void simulation_tick(Simulation* sim, World* world) {
    sim->tick_seed = xorshift32(&sim->rng_state);
    world_clear_tick_flags(world);
    
    // 1. Efektif yogunluklari guncelle (konveksiyon icin)
    buoyancy_update_density(world);
    
    // 2. Basinclari hesapla
    pressure_update(sim, world);
    
    // 3. Kaldirma kuvveti ve konveksiyon
    buoyancy_update(sim, world);
    
    // 4. Powder hareketi
    powder_update(sim, world);
    
    // 5. Fluid hareketi (yuzey gerilimi dahil)
    fluid_update(sim, world);
    
    // 6. Yanma ve atesle ilgili
    combustion_update(sim, world);
    fire_update(sim, world);
    
    // 7. Gaz hareketi
    gas_update(sim, world);
    
    // 8. Asit korozyonu
    acid_update(sim, world);
    
    // 9. Kimyasal reaksiyonlar
    reactions_update(sim, world);
    
    // 10. Termal simulasyon (en son - diger sistemler isi uretir)
    thermal_update(sim, world);
    
    // 11. Oksijen difuzyonu
    combustion_oxygen_diffuse(world);
    
    // 12. Chunk aktivasyonu
    world_update_chunk_activation(world);
    
    sim->tick_count++;
}
```

---

## Faz 9: Performans Optimizasyonlari

### 9.1 Lookup Table'lar

```c
// Onceden hesaplanmis exp() degerleri (Arrhenius icin)
static float EXP_LUT[1000];  // -50 ile 0 arasi

void physics_init_luts(void) {
    for (int i = 0; i < 1000; i++) {
        float x = -50.0f + (float)i * 0.05f;
        EXP_LUT[i] = expf(x);
    }
}

static inline float fast_exp_negative(float x) {
    if (x > 0) return 1.0f;
    if (x < -50.0f) return 0.0f;
    int idx = (int)((x + 50.0f) * 20.0f);
    return EXP_LUT[idx];
}
```

### 9.2 SIMD Optimizasyonu (Gelecek)

```c
// AVX2 ile 8 hucreyi paralel isle
// (Sadece not - simdilik implementasyon yok)
```

### 9.3 Spatial Hash (Sicak Hucreler)

```c
typedef struct {
    int x, y;
    float temperature;
} HotCell;

static HotCell hot_cells[MAX_HOT_CELLS];
static int hot_cell_count = 0;

void thermal_register_hot_cell(int x, int y, float temp) {
    if (temp > HOT_THRESHOLD && hot_cell_count < MAX_HOT_CELLS) {
        hot_cells[hot_cell_count++] = (HotCell){x, y, temp};
    }
}
```

---

## Parametre Referansi

### Termal Sabitler

| Parametre | Deger | Birim | Aciklama |
|-----------|-------|-------|----------|
| AMBIENT_TEMP | 20.0 | C | Ortam sicakligi |
| HEAT_DIFFUSION_RATE | 0.02 | - | Isı yayilim katsayisi |
| RADIATION_SCALE | 1e-8 | - | Radyasyon olcekleme |
| AMBIENT_COOLING_RATE | 0.001 | 1/tick | Ortama soguma |

### Kaldirma Sabitleri

| Parametre | Deger | Birim |
|-----------|-------|-------|
| BUOYANCY_THRESHOLD | 0.001 | m/s^2 |
| CONVECTION_STRENGTH | 0.5 | - |

### Yanma Sabitleri

| Parametre | Deger | Birim |
|-----------|-------|-------|
| ARRHENIUS_A | 1e8 | 1/s |
| OXYGEN_MIN_FOR_COMBUSTION | 0.05 | - |
| OXYGEN_DIFFUSION_RATE | 0.1 | - |
| OXYGEN_REPLENISH_RATE | 0.001 | 1/tick |

### Basinc Sabitleri

| Parametre | Deger | Birim |
|-----------|-------|-------|
| ATM_PRESSURE | 101325 | Pa |
| PRESSURE_THRESHOLD | 100 | Pa |
| PRESSURE_MOVE_FACTOR | 0.001 | - |

---

## Dosya Yapisi

```
include/
├── core/
│   ├── types.h              [DEGISECEK]
│   └── utils.h
├── physics/
│   ├── physics.h            [DEGISECEK]
│   ├── thermal.h            [DEGISECEK]
│   ├── buoyancy.h           [YENI]
│   ├── pressure.h           [YENI]
│   ├── combustion.h         [YENI]
│   ├── surface_tension.h    [YENI]
│   └── energy.h             [YENI]
├── materials/
│   ├── material.h           [DEGISECEK]
│   └── behavior.h           [DEGISECEK]
├── world/
│   ├── world.h              [DEGISECEK]
│   └── ...
└── ...

src/
├── physics/
│   ├── thermal.c            [DEGISECEK]
│   ├── buoyancy.c           [YENI]
│   ├── pressure.c           [YENI]
│   ├── combustion.c         [YENI]
│   ├── surface_tension.c    [YENI]
│   └── energy.c             [YENI]
├── materials/
│   └── material.c           [DEGISECEK]
├── world/
│   └── world.c              [DEGISECEK]
├── subsystems/
│   ├── fire.c               [DEGISECEK]
│   ├── fluid.c              [DEGISECEK]
│   └── powder.c             [DEGISECEK]
└── engine/
    ├── simulation.c         [DEGISECEK]
    └── input.c              [DEGISECEK]
```

---

## Uygulama Takvimi

| Faz | Icerik | Oncelik | Tahmini Satir |
|-----|--------|---------|---------------|
| 1 | Malzeme tanimlari | KRITIK | ~600 |
| 2 | Enerji + Termal | YUKSEK | ~400 |
| 3 | Kaldirma + Konveksiyon | YUKSEK | ~300 |
| 4 | Basinc | ORTA | ~250 |
| 5 | Yuzey gerilimi | ORTA | ~200 |
| 6 | Yanma | YUKSEK | ~500 |
| 7 | Reaksiyonlar | ORTA | ~200 |
| 8 | Pipeline | YUKSEK | ~150 |
| 9 | Optimizasyon | DUSUK | ~200 |

**Toplam: ~2800 satir**

---

*Bu dokuman PHYSICS_PLAN.md olarak kaydedilmistir.*
*Versiyon: 1.0*
*Tarih: 2026-01-19*
