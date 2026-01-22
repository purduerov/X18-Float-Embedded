import board
import analogio
import digitalio
import time

POT_MIN = 0
POT_MAX = 65536
POS_TOL = 0.01
# POT_THRESHOLD = 3000 / POT_MAX

class Actuator:
    def __init__(self, pos_pin = board.A0, extend_pin = board.D12, retract_pin = board.D13):      
        self.pos_pin = analogio.AnalogIn(pos_pin)
        self.extend_pin = digitalio.DigitalInOut(extend_pin)
        self.retract_pin = digitalio.DigitalInOut(retract_pin)

        # intialize motion pins as outputs, and set them to low
        self.extend_pin.direction = digitalio.Direction.OUTPUT
        self.retract_pin.direction = digitalio.Direction.OUTPUT
        self.extend_pin.value = False
        self.retract_pin.value = False
        
        self.moving = 0
        self.move_target = 0.5
        print("Linear actuator initialized")
        
    def get_position(self):
        return self.pos_pin.value / POT_MAX
        
    def tick(self):
        # print("Actuator Direction:", self.moving, ", Position:", self.get_position(), ", Move Target:", self.move_target)
        if self.moving and not ((self.moving * self.get_position()) < (self.moving * self.move_target)):
            self.set_move_pins(0)
        
    def set_move_pins(self, direction):        
        if (direction == 1):
            self.moving = 1
            self.extend_pin.value = True
            self.retract_pin.value = False
        elif (direction == -1):
            self.moving = -1
            self.extend_pin.value = False
            self.retract_pin.value = True
        else:
            self.moving = 0
            self.extend_pin.value = False
            self.retract_pin.value = False
            
    def set_move_target(self, position):
        self.move_target = position
    
    def move_to(self, new_position):
        self.set_move_target(new_position)
        current_position = self.get_position()
        
        if (abs(self.move_target - current_position) < POS_TOL):
            self.set_move_pins(0)
            return False
        
        direction = 1 if new_position > current_position else -1
        if (direction * current_position > direction * new_position):
            print("already moved past position")
            return False
        
        self.set_move_pins(direction)