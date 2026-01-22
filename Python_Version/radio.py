import digitalio
from adafruit_rfm9x import RFM9x 

class Radio:
    def __init__(self, spi, cs, rst, frequency = 915.0, baudrate = 10_000_000):
        self.spi = spi
        self.cs = cs
        self.rst = rst
        self.frequency = frequency
        self.baudrate = baudrate
        
        self.rfm9x = RFM9x(self.spi, digitalio.DigitalInOut(self.cs), digitalio.DigitalInOut(self.rst), self.frequency, baudrate = self.baudrate)
        print("Radio module initialized")
        
    def send(self, data, tx_power = 13):
        self.rfm9x.tx_power = tx_power # range 5-23 dB
        self.rfm9x.send(data)
        
    def receive(self, timeout = 0.5):
        # rec_rssi = self.rfm9x.rssi
        return self.rfm9x.receive(timeout = timeout)