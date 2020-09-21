# requires RPi_I2C_driver.py
import smbus2
import bme280
import RPi_I2C_driver
from time import *

porta_i2c = 1
endereco = 0x76
bus = smbus2.SMBus(porta_i2c)

calibracao_paramentros = bme280.load_calibration_params(bus, endereco)

mylcd = RPi_I2C_driver.lcd()
while(1):
  sleep(1)
  dado = bme280.sample(bus, endereco, calibracao_paramentros)
  mylcd.lcd_display_string("T " + str(round(dado.temperature, 2)) + " U " + str(round(dado.humidity, 2)) ,1)
  mylcd.lcd_display_string("P " + str(round(dado.pressure,2)),2)
