import time
import math
import random

class BuoyancySimulator:
    """
    Simulates the physical dynamics of the buoyancy float in a pool using high-fidelity
    hydrodynamics and electromechanical equations.
    """
    def __init__(self, mass_g=3489, diameter_in=4.5, length_in=12, additional_volume_in3=19.311, syringe_ml=90, pool_depth_ft=15,
                 neutral_adc=2048, act_min=126, act_max=3900,
                 temp_c=20.0, enable_noise=True, sensor_noise_std=0.002, c_added_mass=0.33,
                 beta_p=3.3e-6, alpha_v=6.9e-5, t0_ref=20.0):
        # Physical constants
        self.g = 9.80665  # m/s^2
        
        # User defined specs
        self.mass = mass_g / 1000.0  # kg
        self.diameter = diameter_in * 0.0254  # meters
        self.length = length_in * 0.0254  # meters
        self.syringe_volume = syringe_ml / 1e6  # m^3 (90 mL = 0.00009 m^3)
        self.max_depth = pool_depth_ft * 0.3048  # meters (15 ft = 4.572 m)
        self.additional_volume_in3 = additional_volume_in3
        
        # High-fidelity constants
        self.temp_c = temp_c
        self.enable_noise = enable_noise
        self.sensor_noise_std = sensor_noise_std
        self.Ca = c_added_mass
        self.beta_p = beta_p  # dbar^-1
        self.alpha_v = alpha_v  # K^-1
        self.t0_ref = t0_ref  # C
        
        # Derived physical characteristics
        self.cross_sectional_area = math.pi * ((self.diameter / 2.0) ** 2)  # m^2
        self.Cd = 0.82  # Drag coefficient baseline for cylinder form
        
        # Simulation state variables
        self.depth = 0.0  # meters (0 is surface, positive downwards)
        self.velocity = 0.0  # m/s (positive downwards)
        self.last_update_time = None
        
        # Hardware calibration reference (defaults, can be updated dynamically)
        self.act_min = act_min
        self.act_max = act_max
        self.neutral_adc = neutral_adc
        self.v_hull_zero = 0.0
        
        self.reset()
        
    def reset(self, initial_depth=0.0):
        """Resets the simulation state and recalculates the hull volume baseline."""
        self.depth = initial_depth
        self.velocity = 0.0
        self.last_update_time = time.time()
        
        # Calculate dry/hull volume at surface (depth = 0) based on current calibration
        rho_surface = self.get_water_density(0.0)
        v_neutral = self.mass / rho_surface
        
        adc_range = max(self.act_max - self.act_min, 1)
        neutral_ratio = (self.neutral_adc - self.act_min) / adc_range
        
        # Syringe displacement relative to neutral position
        v_syringe_neutral = neutral_ratio * self.syringe_volume
        self.v_hull_zero = v_neutral - v_syringe_neutral
        
    def set_calibration(self, neutral_adc, act_min, act_max):
        """Updates calibration constants and recalculates hull baseline."""
        self.neutral_adc = neutral_adc
        self.act_min = act_min
        self.act_max = act_max
        # Note: We don't automatically reset depth/velocity here, 
        # but we do update the baseline for the next step().
        rho_surface = self.get_water_density(0.0)
        v_neutral = self.mass / rho_surface
        adc_range = max(self.act_max - self.act_min, 1)
        neutral_ratio = (self.neutral_adc - self.act_min) / adc_range
        self.v_hull_zero = v_neutral - (neutral_ratio * self.syringe_volume)

    def get_water_density(self, depth):
        """Calculates water density based on depth and temperature (UNESCO EOS-80 for S=0)."""
        T = self.temp_c
        # 1. Pure water density at 1 atm (bar = 0)
        rho_w = (999.842594 + 6.793952e-2 * T - 9.095290e-3 * T**2 + 
                 1.001685e-4 * T**3 - 1.120083e-6 * T**4 + 6.536332e-9 * T**5)
        
        # 2. Hydrostatic pressure in bar (1 bar = 10^5 Pa)
        p_bar = (rho_w * self.g * depth) * 1e-5
        
        if p_bar <= 0:
            return rho_w
            
        # 3. Secant Bulk Modulus K for pure water (bar)
        K = (19652.21 + 148.4206 * T - 2.327105 * T**2 + 
             1.360477e-2 * T**3 - 5.155288e-5 * T**4)
             
        # 4. Density under pressure
        return rho_w / (1.0 - p_bar / K)

    def get_water_viscosity(self):
        """Calculates dynamic viscosity of water (Pa·s) as a function of temperature."""
        T = self.temp_c
        if T >= 0:
            power = (1.3272 * (20.0 - T) - 0.001053 * (T - 20.0)**2) / (T + 105.0)
            return 1.002e-3 * 10**power
        return 1.791e-3

    def get_total_volume(self, depth, v_syringe_current):
        """Computes the total volume of the float at a given depth, accounting for compressibility."""
        rho_surface = self.get_water_density(0.0)
        # Pressure in dbar (1 dbar = 10^4 Pa)
        p_dbar = (rho_surface * self.g * depth) * 1e-4
        
        # Hull volume with compression and thermal expansion
        v_hull = self.v_hull_zero * (1.0 - self.beta_p * p_dbar + self.alpha_v * (self.temp_c - self.t0_ref))
        return v_hull + v_syringe_current

    def compute_acceleration(self, depth, velocity, total_volume):
        """Computes instantaneous vertical acceleration including added mass and dynamic drag."""
        rho = self.get_water_density(depth)
        mu = self.get_water_viscosity()
        
        # Added Mass (axial cylinder)
        ma = self.Ca * rho * (self.diameter ** 3)
        
        # Gravity (downward, positive)
        F_gravity = self.mass * self.g
        
        # Buoyancy (upward, negative)
        F_buoyancy = -rho * self.g * total_volume
        
        # Reynolds-dependent drag transition
        re_l = (rho * abs(velocity) * self.length) / mu
        if re_l < 1.0:
            Cf = 1.328
        elif re_l < 5e5:
            Cf = 1.328 / math.sqrt(re_l)
        else:
            Cf = 0.074 / (re_l**0.2) - 1700.0 / re_l
            Cf = max(Cf, 0.003)
            
        # Combine form drag and wetted skin friction
        Cd = self.Cd + Cf * 4.0 * (self.length / self.diameter)
        F_drag = -0.5 * rho * Cd * self.cross_sectional_area * velocity * abs(velocity)
        
        F_net = F_gravity + F_buoyancy + F_drag
        return F_net / (self.mass + ma)

    def step(self, current_adc, dt=None):
        """
        Advances the physics simulation by one time step dt using Runge-Kutta 4th Order (RK4).
        
        current_adc: The current physical ADC position of the syringe (0-4095).
        """
        now = time.time()
        if dt is None:
            if self.last_update_time is None:
                self.last_update_time = now
                return self.depth
            dt = now - self.last_update_time
            # Cap dt to avoid large instability during UI lags
            dt = min(dt, 0.5)
            
        self.last_update_time = now
        
        if dt <= 0:
            return self.depth
            
        # Sub-stepping for solver stability (e.g., 10ms max steps)
        sub_steps = max(1, int(dt / 0.01))
        dt_sub = dt / sub_steps
        
        adc_range = max(self.act_max - self.act_min, 1)
        current_ratio = (current_adc - self.act_min) / adc_range
        
        # Syringe displacement relative to neutral position
        v_syringe_current = current_ratio * self.syringe_volume
        
        for _ in range(sub_steps):
            # 2. Integrate states using 4th Order Runge-Kutta (RK4)
            
            # k1
            v_tot_1 = self.get_total_volume(self.depth, v_syringe_current)
            k1_v = self.compute_acceleration(self.depth, self.velocity, v_tot_1)
            k1_z = self.velocity
            
            # k2
            z_2 = self.depth + k1_z * dt_sub / 2.0
            v_tot_2 = self.get_total_volume(z_2, v_syringe_current)
            k2_v = self.compute_acceleration(z_2, self.velocity + k1_v * dt_sub / 2.0, v_tot_2)
            k2_z = self.velocity + k1_v * dt_sub / 2.0
            
            # k3
            z_3 = self.depth + k2_z * dt_sub / 2.0
            v_tot_3 = self.get_total_volume(z_3, v_syringe_current)
            k3_v = self.compute_acceleration(z_3, self.velocity + k2_v * dt_sub / 2.0, v_tot_3)
            k3_z = self.velocity + k2_v * dt_sub / 2.0
            
            # k4
            z_4 = self.depth + k3_z * dt_sub
            v_tot_4 = self.get_total_volume(z_4, v_syringe_current)
            k4_v = self.compute_acceleration(z_4, self.velocity + k3_v * dt_sub, v_tot_4)
            k4_z = self.velocity + k3_v * dt_sub
            
            # Update state variables
            self.velocity += (dt_sub / 6.0) * (k1_v + 2.0 * k2_v + 2.0 * k3_v + k4_v)
            self.depth += (dt_sub / 6.0) * (k1_z + 2.0 * k2_z + 2.0 * k3_z + k4_z)
            
            # 3. Enforce pool boundaries (surface and bottom)
            if self.depth <= 0.0:
                self.depth = 0.0
                self.velocity = max(0.0, self.velocity)
            elif self.depth >= self.max_depth:
                self.depth = self.max_depth
                self.velocity = min(0.0, self.velocity)
                
        # Return the depth with sensor noise (without modifying the internal state variable self.depth)
        ret_depth = self.depth
        if self.enable_noise and self.sensor_noise_std > 0.0:
            ret_depth += random.gauss(0.0, self.sensor_noise_std)
            # No clipping here: Allow small negative values for realistic noise at surface
            
        return ret_depth
