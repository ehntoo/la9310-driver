#ifndef RFNM_GRANITA_DRIVER_H
#define RFNM_GRANITA_DRIVER_H

#include <linux/spi/spi.h>
#include <linux/types.h>

int granita_driver_reset_defaults(struct spi_device *spi);
int granita_driver_set_rx_gain(struct spi_device *spi, unsigned int path_mask, int gain_idx);
int granita_driver_configure_rx(struct spi_device *spi, unsigned int path_mask, uint64_t freq_khz, uint64_t bw_khz);

#endif