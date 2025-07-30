#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

#include "periphery/mmio.h"

/**
 * @file rcc_freq_reader.c
 * @brief Accesses STM32F4 RCC registers via /dev/mem to detect system clock frequency.
 *
 * This program maps the RCC register space directly from physical memory
 * to user space and reads the PLL configuration to estimate the
 * system clock frequency (SYSCLK). It requires root privileges to run
 * since it accesses `/dev/mem`.
 *
 * References:
 * - STM32F4 Reference Manual RM0090
 * - https://github.com/STMicroelectronics/STM32CubeF4 (Drivers/CMSIS/Device/ST/STM32F4xx/Include/stm32f429xx.h)
 * - https://github.com/STMicroelectronics/cmsis-device-f4
 */

/* -------------------- RCC Register Map -------------------- */

/** @brief Peripheral base address in the alias region */
#define PERIPH_BASE 0x40000000UL

/** @brief AHB1 peripheral base address */
#define AHB1PERIPH_BASE (PERIPH_BASE + 0x00020000UL)

/** @brief RCC base address */
#define RCC_BASE (AHB1PERIPH_BASE + 0x3800UL)

/**
 * @struct RCC_TypeDef
 * @brief Register map structure for the RCC peripheral.
 *
 * This structure mirrors the memory layout of the STM32F4
 * Reset and Clock Control (RCC) registers as documented in RM0090.
 */
typedef struct
{
    volatile uint32_t CR;         /**< RCC clock control register (0x00) */
    volatile uint32_t PLLCFGR;    /**< RCC PLL configuration register (0x04) */
    volatile uint32_t CFGR;       /**< RCC clock configuration register (0x08) */
    volatile uint32_t CIR;        /**< RCC clock interrupt register (0x0C) */
    volatile uint32_t AHB1RSTR;   /**< RCC AHB1 peripheral reset register (0x10) */
    volatile uint32_t AHB2RSTR;   /**< RCC AHB2 peripheral reset register (0x14) */
    volatile uint32_t AHB3RSTR;   /**< RCC AHB3 peripheral reset register (0x18) */
    uint32_t RESERVED0;           /**< Reserved (0x1C) */
    volatile uint32_t APB1RSTR;   /**< RCC APB1 peripheral reset register (0x20) */
    volatile uint32_t APB2RSTR;   /**< RCC APB2 peripheral reset register (0x24) */
    uint32_t RESERVED1[2];        /**< Reserved (0x28-0x2C) */
    volatile uint32_t AHB1ENR;    /**< RCC AHB1 peripheral clock enable register (0x30) */
    volatile uint32_t AHB2ENR;    /**< RCC AHB2 peripheral clock enable register (0x34) */
    volatile uint32_t AHB3ENR;    /**< RCC AHB3 peripheral clock enable register (0x38) */
    uint32_t RESERVED2;           /**< Reserved (0x3C) */
    volatile uint32_t APB1ENR;    /**< RCC APB1 peripheral clock enable register (0x40) */
    volatile uint32_t APB2ENR;    /**< RCC APB2 peripheral clock enable register (0x44) */
    uint32_t RESERVED3[2];        /**< Reserved (0x48-0x4C) */
    volatile uint32_t AHB1LPENR;  /**< RCC AHB1 peripheral clock enable in low-power mode (0x50) */
    volatile uint32_t AHB2LPENR;  /**< RCC AHB2 peripheral clock enable in low-power mode (0x54) */
    volatile uint32_t AHB3LPENR;  /**< RCC AHB3 peripheral clock enable in low-power mode (0x58) */
    uint32_t RESERVED4;           /**< Reserved (0x5C) */
    volatile uint32_t APB1LPENR;  /**< RCC APB1 peripheral clock enable in low-power mode (0x60) */
    volatile uint32_t APB2LPENR;  /**< RCC APB2 peripheral clock enable in low-power mode (0x64) */
    uint32_t RESERVED5[2];        /**< Reserved (0x68-0x6C) */
    volatile uint32_t BDCR;       /**< RCC Backup domain control register (0x70) */
    volatile uint32_t CSR;        /**< RCC clock control & status register (0x74) */
    uint32_t RESERVED6[2];        /**< Reserved (0x78-0x7C) */
    volatile uint32_t SSCGR;      /**< RCC spread spectrum clock generation register (0x80) */
    volatile uint32_t PLLI2SCFGR; /**< RCC PLLI2S configuration register (0x84) */
    volatile uint32_t PLLSAICFGR; /**< RCC PLLSAI configuration register (0x88) */
    volatile uint32_t DCKCFGR;    /**< RCC dedicated clocks configuration register (0x8C) */
} RCC_TypeDef;

/** @brief RCC memory mapping size */
#define RCC_MAP_SIZE sizeof(RCC_TypeDef)

/* -------------------- Global Pointers -------------------- */

/** @brief Pointer to mapped RCC register structure */
volatile RCC_TypeDef *rcc_regs = NULL;

/* -------------------- Utility Functions -------------------- */

/**
 * @brief Reads PLL configuration and computes SYSCLK frequency in Hertz.
 *
 * This function interprets the PLL configuration register values
 * to determine the system clock frequency based on:
 * - VCO input frequency = f_input / PLLM
 * - VCO output frequency = VCO input × PLLN
 * - SYSCLK = VCO output / PLLP
 *
 * @return Estimated SYSCLK frequency in Hertz.
 */
static unsigned long get_sysclk_freq_hz(void)
{
    uint32_t pllcfgr = rcc_regs->PLLCFGR;

    uint32_t pllm = pllcfgr & 0x3F;
    uint32_t plln = (pllcfgr >> 6) & 0x1FF;
    uint32_t pllp = (((pllcfgr >> 16) & 0x3) + 1) * 2;

#define RCC_PLLCFGR_PLLSRC_HSE (0x400000)
    uint32_t pll_source_freq = (pllcfgr & RCC_PLLCFGR_PLLSRC_HSE) ? 8000000ULL : 16000000ULL;

    uint32_t sysclk = (pll_source_freq / pllm) * plln / pllp;
    return (unsigned long)sysclk;
}

/* -------------------- Main Program -------------------- */

/**
 * @brief Main entry point of the program.
 *
 * Maps the RCC registers, reads the PLL configuration,
 * calculates the system clock frequency, and prints it.
 *
 * @param argc Argument count (unused).
 * @param argv Argument values (unused).
 * @return int 0 on success, non-zero on failure.
 */
int main(int argc, char *argv[])
{
    mmio_t *mmio_rcc = mmio_new();
    mmio_open(mmio_rcc, RCC_BASE, sizeof(RCC_TypeDef));

    rcc_regs = (volatile RCC_TypeDef *)mmio_ptr(mmio_rcc);

    unsigned long cpu_freq = get_sysclk_freq_hz();
    printf("Detected CPU frequency: %lu Hz\n", cpu_freq);

    mmio_close(mmio_rcc);
    mmio_free(mmio_rcc);

    return 0;
}
