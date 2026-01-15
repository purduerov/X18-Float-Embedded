
class Bouyancy:
    def __init__(self, ACTUATOR_NEUTRAL_POS):
        print("BOUYANCY SIM MODE")
        self.depth = 0
        self.rate = 0
        self.ACTUATOR_NEUTRAL_POS = ACTUATOR_NEUTRAL_POS
        
    def update(self, dT, actuator_pos):
        self.rate += ((0.56 * (actuator_pos - 0.5)) - (1.257 * self.rate * abs(self.rate)))            
        self.depth += -self.rate * dT
        if (self.depth < 0):
            self.depth = 0
        
    def get_depth(self):
        return self.depth