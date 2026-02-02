#include "rfm9x_python.h"
#include "hardware/gpio.h"
#include <string.h>

// Low-level register access (ported from arduino-LoRa)
static void write_reg(rfm9x_t *rfm, uint8_t addr, uint8_t val) {
    uint8_t buf[2] = { addr | 0x80, val };
    gpio_put(rfm->ss_pin, 0);
    spi_write_blocking(rfm->spi, buf, 2);
    gpio_put(rfm->ss_pin, 1);
}

static uint8_t read_reg(rfm9x_t *rfm, uint8_t addr) {
    uint8_t reg = addr & 0x7F;
    uint8_t val;
    gpio_put(rfm->ss_pin, 0);
    spi_write_blocking(rfm->spi, &reg, 1);
    spi_read_blocking(rfm->spi, 0, &val, 1);
    gpio_put(rfm->ss_pin, 1);
    return val;
}

bool rfm9x_init(rfm9x_t *rfm, spi_inst_t *spi, uint ss, uint rst, float freq) {
    rfm->spi = spi;
    rfm->ss_pin = ss;
    rfm->reset_pin = rst;
    rfm->frequency = freq;
    rfm->node = RH_BROADCAST_ADDRESS;
    rfm->destination = RH_BROADCAST_ADDRESS;
    rfm->identifier = 0;
    rfm->flags = 0;

    gpio_init(rfm->ss_pin);
    gpio_set_dir(rfm->ss_pin, GPIO_OUT);
    gpio_put(rfm->ss_pin, 1);

    gpio_init(rfm->reset_pin);
    gpio_set_dir(rfm->reset_pin, GPIO_OUT);
    
    // Hardware Reset (matches Python reset())
    gpio_put(rfm->reset_pin, 0);
    sleep_us(100);
    gpio_put(rfm->reset_pin, 1);
    sleep_ms(5);

    if (read_reg(rfm, 0x42) != 0x12) return false;

    // Default modem config matching Python __init__
    write_reg(rfm, 0x01, 0x80); // Sleep + LoRa mode
    rfm9x_set_tx_power(rfm, 13);
    rfm9x_set_signal_bandwidth(rfm, 125000);
    rfm9x_set_spreading_factor(rfm, 7);
    write_reg(rfm, 0x1e, read_reg(rfm, 0x1e) | 0x04); // Enable CRC
    
    write_reg(rfm, 0x01, 0x81); // Standby
    return true;
}

// Mimics Python send()
bool rfm9x_send(rfm9x_t *rfm, const uint8_t *data, uint8_t len, uint8_t tx_power) {
    write_reg(rfm, 0x01, 0x81); // Standby
    rfm9x_set_tx_power(rfm, tx_power);
    
    write_reg(rfm, 0x0d, 0x00); // FIFO PTR
    
    // Write 4-byte RadioHead Header
    write_reg(rfm, 0x00, rfm->destination);
    write_reg(rfm, 0x00, rfm->node);
    write_reg(rfm, 0x00, rfm->identifier);
    write_reg(rfm, 0x00, rfm->flags);
    
    // Write Data
    for(int i=0; i<len; i++) write_reg(rfm, 0x00, data[i]);
    write_reg(rfm, 0x22, len + 4);
    
    write_reg(rfm, 0x01, 0x83); // TX Mode
    
    // Wait for TX Done
    while (!(read_reg(rfm, 0x12) & 0x08));
    write_reg(rfm, 0x12, 0xFF); // Clear IRQs
    return true;
}

// Mimics Python receive()
int rfm9x_receive(rfm9x_t *rfm, uint8_t *buffer, uint8_t max_len, float timeout_s, bool with_header) {
    write_reg(rfm, 0x01, 0x85); // RX Mode
    
    uint32_t start_ms = to_ms_since_boot(get_absolute_time());
    uint32_t timeout_ms = (uint32_t)(timeout_s * 1000);
    
    while (!(read_reg(rfm, 0x12) & 0x40)) {
        if (to_ms_since_boot(get_absolute_time()) - start_ms > timeout_ms) return -1;
    }
    
    uint8_t len = read_reg(rfm, 0x13);
    uint8_t current_addr = read_reg(rfm, 0x10);
    write_reg(rfm, 0x0d, current_addr);
    
    uint8_t header[4];
    for(int i=0; i<4; i++) header[i] = read_reg(rfm, 0x00);
    
    // Destination filtering (matches Python receive() logic)
    if (rfm->node != RH_BROADCAST_ADDRESS && header[0] != RH_BROADCAST_ADDRESS && header[0] != rfm->node) {
        write_reg(rfm, 0x12, 0xFF);
        return -1;
    }
    
    uint8_t payload_len = len - 4;
    uint8_t read_len = (payload_len > max_len) ? max_len : payload_len;
    
    if (with_header) {
        memcpy(buffer, header, 4);
        for(int i=0; i<read_len; i++) buffer[i+4] = read_reg(rfm, 0x00);
        read_len += 4;
    } else {
        for(int i=0; i<read_len; i++) buffer[i] = read_reg(rfm, 0x00);
    }
    
    write_reg(rfm, 0x12, 0xFF);
    return read_len;
}

void rfm9x_set_tx_power(rfm9x_t *rfm, int power) {
    if (power > 20) power = 20;
    if (power < 5) power = 5;
    write_reg(rfm, 0x09, 0x80 | (power - 5)); // PA_BOOST
}
void rfm9x_set_signal_bandwidth(rfm9x_t *rfm, long bw) {
    int bw_val;
    if (bw <= 7800) bw_val = 0;
    else if (bw <= 10400) bw_val = 1;
    else if (bw <= 15600) bw_val = 2;
    else if (bw <= 20800) bw_val = 3;
    else if (bw <= 31250) bw_val = 4;
    else if (bw <= 41700) bw_val = 5;
    else if (bw <= 62500) bw_val = 6;
    else if (bw <= 125000) bw_val = 7;
    else if (bw <= 250000) bw_val = 8;
    else bw_val = 9;

    // REG_MODEM_CONFIG_1 is 0x1D. Update bits 7-4.
    write_reg(rfm, 0x1D, (read_reg(rfm, 0x1D) & 0x0F) | (bw_val << 4));
}

void rfm9x_set_spreading_factor(rfm9x_t *rfm, int sf) {
    if (sf < 6) sf = 6;
    else if (sf > 12) sf = 12;

    if (sf == 6) {
        write_reg(rfm, 0x31, 0xC5); // Detection Optimize
        write_reg(rfm, 0x37, 0x0C); // Detection Threshold
    } else {
        write_reg(rfm, 0x31, 0xC3);
        write_reg(rfm, 0x37, 0x0A);
    }

    // REG_MODEM_CONFIG_2 is 0x1E. Update bits 7-4.
    write_reg(rfm, 0x1E, (read_reg(rfm, 0x1E) & 0x0F) | ((sf << 4) & 0xF0));
}