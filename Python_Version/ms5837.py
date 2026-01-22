import time
import struct
import board

MS5837_ADDRESS     = 0x76
OVERSAMPLE         = 0b010     # number of samples to oversample with. value is 256 * 2^(OVERSAMPLE)
FRESHWATER_DENSITY = 997.0474  # density of freshwater (kg/m^3)
GRAVITY            = 9.80665   # gravity, for P = pgh equation
OVERSAMPLE_WAIT    = {         # wait time for oversampling in ms
    0b000: 1,  # 256
    0b001: 2,  # 512
    0b010: 3,  # 1024
    0b011: 5,  # 2048
    0b100: 10, # 4096
    0b101: 20, # 8192
}

MS5837_COMMAND_RESET       = 0b0001_1110               # Command to reset sensor
MS5837_COMMAND_CONVERT_D1  = 0b0100_0000 + OVERSAMPLE  # Command to start pressure conversion
MS5837_COMMAND_CONVERT_D2  = 0b0101_0000 + OVERSAMPLE  # Command to start temperature conversion
MS5837_COMMAND_READ_ADC    = 0b0000_0000               # Command to read ADC values

class MS5837:
    def __init__(self, i2c, address = MS5837_ADDRESS, oversample = OVERSAMPLE, density = FRESHWATER_DENSITY):
        self.i2c = i2c
        self.address = address
        self.c = [ 0, 0, 0, 0, 0, 0 ]
        self.density = density
        self.sample_time = 1.8 * OVERSAMPLE_WAIT[OVERSAMPLE] / 1000.0
        self.temp = 0 
        self.pressure = 0
        
        # initialize sensor
        self._write_command(MS5837_COMMAND_RESET)
        time.sleep(0.1)
        for i in range(0, 6):
            rom_addr = 0b1010_000_0 | ((i + 1) << 1)
            self.c[i] = struct.unpack(">H", self._read(rom_addr))[0]
        # print(bin(struct.unpack(">H", self._read(0b1010_000_0))[0]))
        print("BAR Sensor Initialized ( coefficients", self.c, ")")

    def _write_command(self, command):
        while not self.i2c.try_lock():
            pass
        try:
            self.i2c.writeto(self.address, bytes([command]))
        finally:
            self.i2c.unlock()
        
    def _read(self, command, length=2):
        while not self.i2c.try_lock():
            pass
        
        result = bytearray(length)
        self.i2c.writeto_then_readfrom(self.address, bytes([command]), result)
        self.i2c.unlock()
        return result

    def _read_adc(self):
        time.sleep(self.sample_time)
        result = self._read(MS5837_COMMAND_READ_ADC, 3)
        return (result[0] << 16) | (result[1] << 8) | result[2]
    
    def read(self):
        self._write_command(MS5837_COMMAND_CONVERT_D1)  # Convert raw pressure
        d1 = self._read_adc()
        
        self._write_command(MS5837_COMMAND_CONVERT_D2)  # Convert raw temperature
        d2 = self._read_adc()
        
        # Calculate temperature
        dT = d2 - (self.c[4] * 256)
        T1 = (2000 + ((dT * self.c[5]) / 8388608))
        
        # calculate pressure
        OFF = (self.c[1] * 65536) + ((self.c[3] * dT) / 128)
        SENS = (self.c[0] * 32768) + ((self.c[2] * dT) / 256)
        P1 = ((d1 * SENS / 2097152) - OFF) / 8192
        
        # second order temperature compensation
        if ((T1 / 100) < 20): # low temp
            Ti = (3 * (dT ** 2)) / 8589934592
            OFFi = (3 * ((T1 - 2000) ** 2)) / 2
            SENSi = (5 * ((T1 - 2000) ** 2)) / 8
            
            if ((T1 / 100) < -15): # very low temp
                OFFi = OFFi + (7 * ((T1 + 1500) ** 2))
                SENSi = SENSi + (4 * ((T1 + 1500) ** 2))
        else: # high temp
            Ti = (2 * (dT ** 2)) / 137438953472
            OFFi = (1 * ((T1 - 2000) ** 2)) / 16
            SENSi = 0
            
        OFF2 = OFF - OFFi
        SENS2 = SENS - SENSi
        
        T2 = T1 - Ti
        P2 = ((d1 * SENS2 / 2097152) - OFF2) / 8192
        
        self.temp = T2 / 100.0
        self.pressure = P2 / 10.0
        
    def get_temperature(self): # temperature, deg C
        return self.temp

    def get_pressure(self): # pressure, mBar
        return self.pressure
    
    def get_depth(self, surface_pressure): # depth, m
        return ((self.pressure - surface_pressure) * 100) / (self.density * GRAVITY)
    
if __name__ == "__main__":
    bus = board.I2C()
    sensor = MS5837(bus, address = 0x76, oversample = 0b101)