def main():
    import board
    import busio
    import time
    import neopixel

    from scheduler import Scheduler
    from ms5837 import MS5837
    from actuator import Actuator
    from radio import Radio
    from pid import PID

    DEPTH_TOL = 0
    ACTUATOR_NEUTRAL_POS = 0.5
    SENSOR_TOP_OFFSET = 0.465

    # from bouyancy_sim import Bouyancy
    # bouyancy = Bouyancy(ACTUATOR_NEUTRAL_POS)

    from adafruit_bno08x.i2c import BNO08X_I2C
    from adafruit_bno08x import (
        BNO_REPORT_ACCELEROMETER,
        BNO_REPORT_GYROSCOPE,
        BNO_REPORT_MAGNETOMETER,
        BNO_REPORT_ROTATION_VECTOR,
    )
    
    led = neopixel.NeoPixel(board.NEOPIXEL, 1)
    led.brightness = 0.3
    led.fill((0, 255, 0))

    actuator = Actuator()

    i2c = board.I2C()
    spi = board.SPI()

    try: 
        ms5837 = MS5837(i2c, address = 0x76, oversample = 2)
    except OSError:
        led.fill((255, 0, 0))
        print("Failed to detect Bar Depth Sensor!")
        exit()
    
    # create baseline pressure for depth reading
    surface_pressure_num_samples = 100
    surface_pressure = 0
    for i in range(surface_pressure_num_samples):
        ms5837.read()
        surface_pressure += ms5837.get_pressure()
    surface_pressure /= surface_pressure_num_samples
    print(f"Surface pressure: {surface_pressure} mbar")
    
    radio = Radio(spi, board.D24, board.D25)

    bno085 = BNO08X_I2C(i2c)
    bno085.enable_feature(BNO_REPORT_ACCELEROMETER)
    bno085.enable_feature(BNO_REPORT_GYROSCOPE)
    bno085.enable_feature(BNO_REPORT_MAGNETOMETER)
    bno085.enable_feature(BNO_REPORT_ROTATION_VECTOR)
    print("IMU initialized")
    
    led.fill((0, 0, 255))

    depth_target = 0
    float_time = 0

    def move_wait(pos, tol = 0.02):
        actuator.move_to(pos)
        while abs(actuator.get_position() - pos) > tol:
            time.sleep(0.1)
            continue
        actuator.set_move_pins(0)
    
    print("Moving actuator to top")
    move_wait(0)
    led.fill((255, 255, 0))
    print("Moving actuator to bottom")
    move_wait(1)
    led.fill((0, 255, 255))
    print("Moving actuator to neutral")
    move_wait(0.5)
    led.fill((255, 255, 255))
    
    Kp = 0.05
    Ki = 0
    Kd = 0.51
    dT = 0.1

    pid = PID(Kp, Ki, Kd, dT, -0.05, 0.05)

    team_number = 01
    def send_radio_message():
        message = f"{float_time : .1f},{ms5837.get_pressure() : .2f},{ms5837.get_depth(surface_pressure) - SENSOR_TOP_OFFSET : .2f}"
        print(message)
        radio.send(message)

    scheduler = Scheduler()
    scheduler.add_task(ms5837.read, 1)
    scheduler.add_task(actuator.tick, 5)
    scheduler.add_task(send_radio_message, 5)

    while True:
        start_time = time.monotonic_ns()
        # accel_x, accel_y, accel_z = bno085.acceleration
        # gyro_x, gyro_y, gyro_z = bno085.gyro
        # mag_x, mag_y, mag_z = bno085.magnetic
        # quat_i, quat_j, quat_k, quat_real = bno085.quaternion
        
        # print(f"Temperature: {ms5837.get_temperature():.2f} C, Pressure: {ms5837.get_pressure():.2f} mBar, Depth: {ms5837.get_depth():.2f} m")
        # print("Accel X: %0.6f  Y: %0.6f Z: %0.6f  m/s^2" % (accel_x, accel_y, accel_z))
        # print("X: %0.6f  Y: %0.6f Z: %0.6f rads/s" % (gyro_x, gyro_y, gyro_z))
        # print("X: %0.6f  Y: %0.6f Z: %0.6f uT" % (mag_x, mag_y, mag_z))
        # print("I: %0.6f  J: %0.6f K: %0.6f  Real: %0.6f" % (quat_i, quat_j, quat_k, quat_real))
        
        current_depth = ms5837.get_depth(surface_pressure) - SENSOR_TOP_OFFSET
        # current_depth = bouyancy.get_depth()
            
        depth_error = depth_target - current_depth
        if (abs(depth_error) > DEPTH_TOL):
            delta = -pid.update(depth_error)
        else:
            delta = 0
        
        new_pos = actuator.move_target + delta
        if new_pos < 0.05:
            new_pos = 0.05
        elif new_pos > 0.95:
            new_pos = 0.95
        actuator.move_to(new_pos)
        
        # bouyancy.update(dT / 10, actuator.get_position())
        scheduler.tick()
        
        elapsed_time = (time.monotonic_ns() - start_time) / 1e9
        float_time += dT
        
        try:
            time.sleep(dT - elapsed_time)
        except ValueError:
            time.sleep((dT * 10) - elapsed_time)
            float_time += dT * 10
        
        # print(f"it {elapsed_time: .5f} s, target {depth_target : .3f} m, current {current_depth : .3f} m, float_time = {float_time : .1f} s")
        
        if (float_time > 200):
            depth_target = -5
            led.fill((0, 0, 255))
        elif (float_time > 100):
            depth_target = 0.05
            led.fill((255, 0, 0))
        elif (float_time > 10):
            depth_target = 1
            led.fill((0, 255, 0))
     
if __name__ == "__main__":
    main()
