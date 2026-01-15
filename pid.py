import time

NEUTRAL_ACTUATOR_POS = 0.5  # steps/mm/etc where float is neutrally buoyant
ACTUATOR_MIN = 0            # mechanical minimum
ACTUATOR_MAX = 1            # mechanical maximum

class PID:
    def __init__(self, kp = 50.0, ki = 0.5, kd = 5.0, dt = 0.1, output_min = -0.5, output_max = 0.5):
        self.kp = kp
        self.ki = ki
        self.kd = kd
        self.dt = dt

        self.target_depth = 0
        
        self.integral = 0.0
        self.prev_error = 0.0

        self.output_min = output_min
        self.output_max = output_max

    def reset(self):
        self.integral = 0.0
        self.prev_error = 0.0

    def update(self, error):
        # Proportional term
        P = self.kp * error

        # Integral term (with basic anti-windup via clamping)
        self.integral += error * self.dt
        I = self.ki * self.integral

        # Derivative term
        derivative = (error - self.prev_error) / self.dt
        D = self.kd * derivative

        # PID output
        output = P + I + D

        # Store error for next derivative calc
        self.prev_error = error

        # Clamp output if limits given
        if self.output_min is not None:
            output = max(self.output_min, output)
        if self.output_max is not None:
            output = min(self.output_max, output)

        return output