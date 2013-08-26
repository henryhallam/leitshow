/*
Copyright 2013 Cosmogia
*/

#include <stdio.h>
#include <string.h>

// External dependencies
#include <libopencm3/stm32/f4/adc.h>
#include <libopencm3/stm32/f4/nvic.h>
#include <libopencm3/stm32/f4/rcc.h>
#include <libopencm3/stm32/f4/dma.h>
#include <libopencm3/stm32/f4/gpio.h>

// Drivers
#include "./util.h"
#include "./adc.h"


// Configuration variables
static bool GLOBAL_ADC_DEBUG_FLAG = false;
static uint16_t GLOBAL_ADC_DMA_BUFFER[16];

// This is a structure of configuration settings for the DMA
typedef struct {
  uint32_t dma;
  uint8_t stream;
  uint32_t channel;
  volatile uint32_t *source;
  uint16_t *destination;
  uint8_t number_of_data;
} dma_config_t;

// This is a structure of configuration settings for the ADC
typedef struct {
  uint32_t adc;
  uint8_t channels[8];
  uint8_t num_channels;
  dma_config_t D;         // <--- notice it includes DMA configuration
} adc_configuration_t;


// ------------------------------------------------------------------ Static
// Enable Vbat (should be in libopemcm3 library)
static void adc_enable_vbat_sensor(void) {
  ADC_CCR |= ADC_CCR_VBATE;
}

static void dma_init(dma_config_t d) {
  // Straight from the manual... The following sequence should be followed to
  // configure a DMA stream x (where x is the stream number):
  // 1. If the stream is enabled, disable it by resetting the EN bit in the
  //    DMA_SxCR register, then read this bit in order to confirm that there is
  //    no ongoing stream operation. Writing  this bit to 0 is not immediately
  //    effective since it is actually written to 0 once all the current
  //    transfers have finished. When the EN bit is read as 0, this means that
  //    the stream is ready to be configured. It is therefore necessary to wait
  //    for the EN bit to be cleared before starting any stream configuration.
  //    All the stream dedicated bits set in the status register (DMA_LISR and
  //    DMA_HISR) from the previous data block DMA transfer should be cleared
  //    before the stream can be re-enabled.
  dma_stream_reset(d.dma, d.stream);
  dma_disable_stream(d.dma, d.stream);
  // 2. Set the peripheral port register address in the DMA_SxPAR register. The
  //    data will be moved from/ to this address to/ from the peripheral port
  //    after the peripheral event.
  dma_set_peripheral_address(d.dma, d.stream, (uint32_t)d.source);
  // 3. Set the memory address in the DMA_SxMA0R register (and in the DMA_SxMA1R
  //    register in the case of a double buffer mode). The data will be written
  //    to or read from
  //  this memory after the peripheral event.
  dma_set_memory_address(d.dma, d.stream, (uint32_t)d.destination);
  // 4. Configure the total number of data items to be transferred in the
  //    DMA_SxNDTR register. After each peripheral event or each beat of the
  //    burst, this value is
  //  decremented.
  dma_set_number_of_data(d.dma, d.stream, d.number_of_data);
  // 5. Select the DMA channel (request) using CHSEL[2:0] in the DMA_SxCR
  //    register.
  dma_channel_select(d.dma, d.stream, d.channel);
  // 6. If the peripheral is intended to be the flow controller and if it
  //    supports this feature, set the PFCTRL bit in the DMA_SxCR register.
  // Nope
  // 7. Configure the stream priority using the PL[1:0] bits in the DMA_SxCR
  //    register.
  dma_set_priority(d.dma, d.stream, DMA_SxCR_PL_MEDIUM);
  // 8. Configure the FIFO usage (enable or disable, threshold in transmission
  //    and reception) Leave as default
  // 9. Configure the:
  //    - data transfer direction,
  dma_set_transfer_mode(d.dma, d.stream, DMA_SxCR_DIR_PERIPHERAL_TO_MEM);
  //    - peripheral and memory incremented/fixed mode,
  dma_disable_peripheral_increment_mode(d.dma, d.stream);
  dma_enable_memory_increment_mode(d.dma, d.stream);
  //    - single or burst transactions,
  // Single is default
  //    - peripheral and memory data widths,
  dma_set_memory_size(d.dma, d.stream, DMA_SxCR_MSIZE_16BIT);
  dma_set_peripheral_size(d.dma, d.stream, DMA_SxCR_PSIZE_16BIT);
  //    - Circular mode,
  dma_enable_circular_mode(d.dma, d.stream);
  //    - Double buffer mode
  // Not enabled
  //    - interrupts after half and/or full transfer,
  // Not yet...
  //    - and/or errors in the DMA_SxCR register.
  // Not yet...
  // 10. Activate the stream by setting the EN bit in the DMA_SxCR register.
  dma_enable_stream(d.dma, d.stream);
}

// This function finishes configuring the ADC according to the given
// configuration struct
static void adc_dynamic_init(adc_configuration_t c) {
  // The start of the configuration happens in boardsupport
  // 4. Setup channels
  adc_set_regular_sequence(c.adc, c.num_channels, c.channels);
  // 5. Set single/continouos/discontinouos mode
  adc_set_single_conversion_mode(c.adc);
  // adc_set_continuous_conversion_mode(c.adc);
  adc_enable_discontinuous_mode_regular(c.adc, c.num_channels);
  // 6. Enable/Disable analog watchdog
  adc_disable_analog_watchdog_regular(c.adc);
  adc_disable_analog_watchdog_injected(c.adc);
  // 7. Enable/disable scan mode
  adc_enable_scan_mode(c.adc);
  // 8. Set the alignment of data
  adc_set_right_aligned(c.adc);
  // 9. Set the sample time
  adc_set_sample_time_on_all_channels(c.adc, ADC_SMPR_SMP_3CYC);
  // 10. Fast conversion mode?
  // N/A
  // 11. Configure DMA
  adc_set_dma_continue(c.adc);
  adc_enable_dma(c.adc);
  // 12. Set Multi-ADC mode
  adc_set_multi_mode(ADC_CCR_MULTI_INDEPENDENT);
  // 13. Enable the temperature sensor / battery if using
  adc_enable_temperature_sensor();
  adc_enable_vbat_sensor();
  // 14. Turn it on
  adc_power_on(c.adc);
  // 15. Connect it to dma (Note: DMA clocks must be seperately turned on)
  dma_init(c.D);
}

/// Initialization for adc
static void adc_init(void) {
  // This function sets up the ADC1 and ADC3 configuration options.
  // There's several "magic numbers" used in the configuration, so it's
  // pretty fragile.  Don't change anything here unless you have to.
  adc_configuration_t test_adc1 = {
    ADC1,           // Name of ADC peripheral
    {ADC_CHANNEL8, ADC_CHANNEL9},   // Listing of Channels
    2,                                        // Number of channels
    {
      DMA2,                         // Associated DMA
      DMA_STREAM0,                  // Associated DMA Stream
      DMA_SxCR_CHSEL_0,             // DMA Channel Select
      (volatile uint32_t *) &ADC1_DR,         // Pointer to ADC Data Register
      (uint16_t *) &GLOBAL_ADC_DMA_BUFFER[0], // Pointer to the DMA memory target
      6                             //
    }
  };

  adc_dynamic_init(test_adc1);

}

// ------------------------------------------------------------------- Public
// TODO(Mike2): Make this dependent on the uint32_t ADC channel
void adc_setup(void) {
  // The goal of this sequence is to have the adc running in regular mode,
  // polling channels continouosly and transferring the results to SRAM
  // via DMA.

  // 1. Turn off ADC for configuration
  adc_off(ADC1);

  // 2. Set the pin configuration
  //    Note that we're setting all these pins to be analog pins, no pull up or
  //    pull down
  rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPBEN);
  gpio_mode_setup(GPIOB, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO0 | GPIO1);    // B0 and B1

  // 3. Setup the clocks
  rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_ADC1EN);
  adc_set_clk_prescale(ADC_CCR_ADCPRE_BY2);
  rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_DMA2EN);


  // The rest of the configuration is handled in adc_init()
  adc_init();
}


void adc_poll() {
  adc_start_conversion_regular(ADC1);
}


void adc_get_dma_results(uint16_t results[2]) {
  results[0] = GLOBAL_ADC_DMA_BUFFER[0];
  results[1] = GLOBAL_ADC_DMA_BUFFER[1];
}
