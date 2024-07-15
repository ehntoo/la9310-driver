#include "granita_driver.h"

#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/types.h>

struct default_reg_setting {
    uint8_t  full_reg_addr;
    uint8_t  channel_idx;
    uint8_t  pad_3[6];
    uint32_t value;
    uint8_t  pad_12[4];
};

struct granita_reg_default_settings {
    uint8_t channel_address_prefix[0x10];
    struct default_reg_setting default_settings[];
};

struct rx_gain_entry {
    uint8_t reg_0x05_b_12_11;
    uint8_t reg_0x05_b_10_09;
    uint8_t reg_0x05_b_08_04;
    uint8_t reg_0x05_b_03_00;
    uint8_t reg_0x05_b_23_16;
    uint8_t reg_0x13_b_21_15;
    uint8_t reg_0x13_b_14_08;
    uint8_t reg_0x13_b_7_0;
};

extern const struct granita_reg_default_settings STCfgGeneral;
static const size_t num_default_settings = 0x79;

extern const struct rx_gain_entry RX_Gain_LUT[0x4f];

static const size_t granita_num_channels = 4;

static int granita_spi_read(struct spi_device *spi, uint8_t addr_prefix, uint8_t reg_addr, uint32_t *out_value)
{
    const uint8_t granita_reg_read = 0x00;
	struct spi_transfer xfer;
    uint8_t rxbuf[6];
	const uint8_t txbuf[6] = {
        addr_prefix | granita_reg_read,
        reg_addr
    };

    memset(&xfer, 0, sizeof(xfer));
    xfer.tx_buf = txbuf;
    xfer.rx_buf = rxbuf;
    xfer.len = 6;
    int ret = spi_sync_transfer(spi, &xfer, 1);
    if (ret) return ret;

    *out_value = (rxbuf[2] << 24u) | (rxbuf[3] << 16u) | (rxbuf[4] << 8u) | (rxbuf[5] << 0u);
    return 0;
}

static int granita_spi_write_checked(struct spi_device *spi, uint8_t addr_prefix, uint8_t reg_addr, uint32_t value)
{
    const uint8_t granita_reg_write = 0x80;
	struct spi_transfer xfer;
	uint8_t txbuf[6] = {
        addr_prefix | granita_reg_write,
        reg_addr,
        (value >> 24) & 0xFF,
        (value >> 16) & 0xFF,
        (value >> 8) & 0xFF,
        (value >> 0) & 0xFF
    };

    memset(&xfer, 0, sizeof(xfer));
    xfer.tx_buf = txbuf;
    xfer.len = 6;
    int ret = spi_sync_transfer(spi, &xfer, 1);
    if (ret) return ret;

    uint32_t readback_val;
    ret = granita_spi_read(spi, addr_prefix, reg_addr, &readback_val);
    if (ret) return ret;
    
    if (readback_val == value) return 0;
    return -1;
}

static int granita_spi_write_unchecked(struct spi_device *spi, uint8_t addr_prefix, uint8_t reg_addr, uint32_t value)
{
    const uint8_t granita_reg_write = 0x80;
	struct spi_transfer xfer;
	const uint8_t txbuf[6] = {
        addr_prefix | granita_reg_write,
        reg_addr,
        (value >> 24) & 0xFF,
        (value >> 16) & 0xFF,
        (value >> 8) & 0xFF,
        (value >> 0) & 0xFF
    };

    memset(&xfer, 0, sizeof(xfer));
    xfer.tx_buf = txbuf;
    xfer.len = 6;
    int ret = spi_sync_transfer(spi, &xfer, 1);
    if (ret)
        return ret;
    return 0;
}

int granita_driver_reset_defaults(struct spi_device *spi)
{
    for (size_t i = 0; i < num_default_settings; i++) {
        int ret = granita_spi_write_checked(spi, STCfgGeneral.channel_address_prefix[STCfgGeneral.default_settings[i].channel_idx], STCfgGeneral.default_settings[i].full_reg_addr, STCfgGeneral.default_settings[i].value);
        if (ret) return ret;
    }
    return 0;
}


int granita_driver_set_rx_gain(struct spi_device *spi, unsigned int path_mask, int gain_idx)
{
    if (!(path_mask & ((1 << granita_num_channels) - 1))) return 0;

    if (gain_idx > sizeof(RX_Gain_LUT)/sizeof(RX_Gain_LUT[0])) return -1;
    const struct rx_gain_entry *rg = &RX_Gain_LUT[gain_idx];

    const uint32_t gain_val_0x05 = (rg->reg_0x05_b_23_16 << 16) |
                                    (rg->reg_0x05_b_12_11 << 11) | 
                                    (rg->reg_0x05_b_10_09 << 10) | 
                                    (rg->reg_0x05_b_08_04 << 4) | 
                                    (rg->reg_0x05_b_03_00 << 0);
    const uint32_t gain_val_0x13 = (rg->reg_0x13_b_21_15 << 15) |
                                    (rg->reg_0x13_b_14_08 << 8) |
                                    (rg->reg_0x13_b_7_0 << 0);

    const uint32_t enable_disable_mask = (1 << 9) | (1 << 4);
    const uint32_t reg_0x05_gain_mask = ((1 << 0xd) - 1) | (((1 << 8) - 1) << 0x10);
    const uint32_t reg_0x13_gain_mask = (((1 << 7) - 1) << 0xF) | ((1 << 0xF) - 1);

    uint32_t system_reg_0x57_orig_val;
    int ret = granita_spi_read(spi, STCfgGeneral.channel_address_prefix[0], 0x57, &system_reg_0x57_orig_val);
    if (ret) return ret;

    for (size_t i=0; i < granita_num_channels; i++) {
        if (!(path_mask & (1 << i))) {
            continue;
        }

        ret = granita_spi_write_unchecked(spi, STCfgGeneral.channel_address_prefix[0], 0x57, system_reg_0x57_orig_val & ~(1 << (0x11 - i)));
        if (ret) return ret;

        uint32_t chan_reg_0x15_orig_val;
        ret = granita_spi_read(spi, STCfgGeneral.channel_address_prefix[i], 0x15, &chan_reg_0x15_orig_val);
        if (ret) return ret;

        ret = granita_spi_write_unchecked(spi, STCfgGeneral.channel_address_prefix[i], 0x15, chan_reg_0x15_orig_val & ~enable_disable_mask);
        if (ret) return ret;

        // TODO: Why does the vendor driver do this? 10msec is a long time to
        // sleep. We should see if anything looks like a status byte that
        // sets/clears for the power-down/disable process
        msleep(10);

        uint32_t chan_reg_0x05_orig_val;
        ret = granita_spi_read(spi, STCfgGeneral.channel_address_prefix[i], 0x05, &chan_reg_0x05_orig_val);
        if (ret) return ret;

        ret = granita_spi_write_unchecked(spi, STCfgGeneral.channel_address_prefix[i], 0x05,
            (chan_reg_0x05_orig_val & ~reg_0x05_gain_mask) | gain_val_0x05);
        if (ret) return ret;

        uint32_t chan_reg_0x13_orig_val;
        ret = granita_spi_read(spi, STCfgGeneral.channel_address_prefix[i], 0x13, &chan_reg_0x13_orig_val);
        if (ret) return ret;

        ret = granita_spi_write_unchecked(spi, STCfgGeneral.channel_address_prefix[i], 0x13,
            (chan_reg_0x13_orig_val & ~reg_0x13_gain_mask) | gain_val_0x13);
        if (ret) return ret;

        // TODO: Why does the vendor driver do this? 20msec is a crazy long time
        // to sleep. See above.
        msleep(20);

        ret = granita_spi_write_unchecked(spi, STCfgGeneral.channel_address_prefix[i], 0x15, chan_reg_0x15_orig_val | enable_disable_mask);
        if (ret) return ret;
    
        ret = granita_spi_write_unchecked(spi, STCfgGeneral.channel_address_prefix[0], 0x57, system_reg_0x57_orig_val);
        if (ret) return ret;
    }
    return 0;
}

int granita_driver_configure_rx(struct spi_device *spi, unsigned int path_mask, uint64_t freq_khz, uint64_t bw_khz)
{
    return 0;
}
