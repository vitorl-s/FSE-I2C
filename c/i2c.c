#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include <time.h>

#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#include "bme280.h"

struct identifier {
    uint8_t dev_addr;
    int8_t fd;
};

void user_delay_us(uint32_t period, void *intf_ptr) {
    usleep(period);
}


int8_t i2c_read(uint8_t reg_addr, uint8_t *data, uint32_t len, void *intf_ptr) {
    struct identifier id;

    id = *((struct identifier *)intf_ptr);

    write(id.fd, &reg_addr, 1);
    read(id.fd, data, len);

    return 0;
}

int8_t i2c_write(uint8_t reg_addr, const uint8_t *data, uint32_t len, void *intf_ptr) {
    uint8_t *buf;
    struct identifier id;

    id = *((struct identifier *)intf_ptr);

    buf = malloc(len + 1);
    buf[0] = reg_addr;
    memcpy(buf + 1, data, len);
    if (write(id.fd, buf, len + 1) < (uint16_t)len) {
        return BME280_E_COMM_FAIL;
    }

    free(buf);

    return BME280_OK;
}

int8_t calibrate_sensor(struct bme280_dev *dev) {
    int8_t result = BME280_OK;
    uint8_t settings_sel = 0;

    dev->settings.osr_h = BME280_OVERSAMPLING_1X;
    dev->settings.osr_p = BME280_OVERSAMPLING_16X;
    dev->settings.osr_t = BME280_OVERSAMPLING_2X;
    dev->settings.filter = BME280_FILTER_COEFF_16;

    settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL;

    result = bme280_set_sensor_settings(settings_sel, dev);
    return result;
}

void save_measurements(double temp, double humidity, double pressure) {
    FILE *fp = fopen("data_results", "a+");
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char timestring[64];
    strftime(timestring, sizeof(timestring), "%c", tm);
    printf("Measures saved: %s\n", timestring);
    fprintf(fp, "[%s] - Temperature: %0.2lf C°, Humidity: %0.2lf%%, Pressure: %0.2lf hPa\n", timestring, temp, humidity, pressure * 0.01);
    fclose(fp);
}

int8_t stream_sensor_data(struct bme280_dev *dev) {
    int8_t result = BME280_OK;
    uint32_t req_delay;

    struct bme280_data data;

    result = calibrate_sensor(dev);
    if (result != BME280_OK) {
        fprintf(stderr, "Error in set sensor settings. Code %+d\n", result);
        return result;
    }

    req_delay = bme280_cal_meas_delay(&dev->settings);

    int count = 0;
    double temp_sum = 0;
    double humidity_sum = 0;
    double pressure_sum = 0;

    while (1) {
        count += 1;

        result = bme280_set_sensor_mode(BME280_FORCED_MODE, dev);
        if (result != BME280_OK){
            fprintf(stderr, "Failed to set sensor mode (code %+d).", result);
            break;
        }

        dev->delay_us(req_delay, dev->intf_ptr);
        result = bme280_get_sensor_data(BME280_ALL, &data, dev);
        if (result != BME280_OK){
            fprintf(stderr, "Failed to get sensor data (code %+d).", result);
            break;
        }

        temp_sum += (&data)->temperature;
        humidity_sum += (&data)->humidity;
        pressure_sum += (&data)->pressure;

        if (count == 10) {
            temp_sum = temp_sum / 10.0;
            humidity_sum = humidity_sum / 10.0;
            pressure_sum = pressure_sum / 10.0;

            save_measurements(temp_sum, humidity_sum, pressure_sum);
            
            count = 0;
            temp_sum = 0;
            humidity_sum = 0;
            pressure_sum = 0;
        }
        sleep(1);
    }

}


int main(int argc, char *argv[]) {
    struct bme280_dev dev;
    struct identifier id;

    int8_t result = BME280_OK;

    id.dev_addr = BME280_I2C_ADDR_PRIM;
    id.fd = open("/dev/i2c-1", O_RDWR);

    if (id.fd < 0) {
        fprintf(stderr, "Error trying to open bus");
        exit(1);
    }

    if (ioctl(id.fd, I2C_SLAVE, id.dev_addr) < 0){
        fprintf(stderr, "Error at communicating with bus");
        exit(1);
    }

    dev.intf = BME280_I2C_INTF;
    dev.read = i2c_read;
    dev.write = i2c_write;
    dev.delay_us = user_delay_us;

    dev.intf_ptr = &id;

    result = bme280_init(&dev);
    if (result != BME280_OK) {
        fprintf(stderr, "Failed to init device. Code %+d\n", result);
        exit(1);
    }

    result = stream_sensor_data(&dev);
    if (result != BME280_OK) {
        fprintf(stderr, "Error in data streaming. Code %+d\n", result);
        exit(1);
    }

    close(id.fd);

    return 0;
}
