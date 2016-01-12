// ------------------------------------------------------------
// PMU for Cortex-A/R (v7-A/R)
// ------------------------------------------------------------

#ifndef _V7_PMU_H
#define _V7_PMU_H

// Returns the number of progammable counters
unsigned int av_arm_getPMN(void);

// Sets the event for a programmable counter to record
// counter = r0 = Which counter to program  (e.g. 0 for PMN0, 1 for PMN1)
// event   = r1 = The event code (from appropiate TRM or ARM Architecture Reference Manual)
void av_arm_pmn_config(unsigned int counter, unsigned int event);

// Enables/disables the divider (1/64) on CCNT
// divider = r0 = If 0 disable divider, else enable dvider
void av_arm_ccnt_divider(int divider);

//
// Enables and disables
//

// Global PMU enable
// On ARM11 this enables the PMU, and the counters start immediately
// On Cortex this enables the PMU, there are individual enables for the counters
void av_arm_enable_pmu(void);

// Global PMU disable
// On Cortex, this overrides the enable state of the individual counters
void av_arm_disable_pmu(void);

// Enable the CCNT
void av_arm_enable_ccnt(void);

// Disable the CCNT
void av_arm_disable_ccnt(void);

// Enable PMN{n}
// counter = The counter to enable av_arm_(e.g. 0 for PMN0, 1 for PMN1)
void av_arm_enable_pmn(unsigned int counter);

// Enable PMN{n}
// counter = The counter to enable av_arm_(e.g. 0 for PMN0, 1 for PMN1)
void av_arm_disable_pmn(unsigned int counter);

//
// Read counter values
//

// Returns the value of CCNT
static av_always_inline unsigned int av_arm_read_ccnt(void)
{
    unsigned int rv;
    __asm__ volatile (
		"MRC     p15, 0, %[rv], c9, c13, 0  \n\t"
        : [rv]"=r"(rv)
        : // Inputs (null)
        : // Clobbers (null)
        );
    return rv;
}

// Returns the value of PMN{n}
// counter = The counter to read av_arm_(e.g. 0 for PMN0, 1 for PMN1)
unsigned int av_arm_read_pmn(unsigned int counter);

//
// Overflow and interrupts
//

// Returns the value of the overflow flags
unsigned int av_arm_read_flags(void);

// Writes the overflow flags
void av_arm_write_flags(unsigned int flags);

// Enables interrupt generation on overflow of the CCNT
void av_arm_enable_ccnt_irq(void);

// Disables interrupt generation on overflow of the CCNT
void av_arm_disable_ccnt_irq(void);

// Enables interrupt generation on overflow of PMN{x}
// counter = The counter to enable the interrupt for av_arm_(e.g. 0 for PMN0, 1 for PMN1)
void av_arm_enable_pmn_irq(unsigned int counter);

// Disables interrupt generation on overflow of PMN{x}
// counter = r0 =  The counter to disable the interrupt for av_arm_(e.g. 0 for PMN0, 1 for PMN1)
void av_arm_disable_pmn_irq(unsigned int counter);

//
// Counter reset functions
//

// Resets the programmable counters
void av_arm_reset_pmn(void);

// Resets the CCNT
void av_arm_reset_ccnt(void);

//
// Software Increment

// Writes to software increment register
// counter = The counter to increment av_arm_(e.g. 0 for PMN0, 1 for PMN1)
void av_arm_pmu_software_increment(unsigned int counter);

//
// User mode access
//

// Enables User mode access to the PMU av_arm_(must be called in a priviledged mode)
void av_arm_enable_pmu_user_access(void);

// Disables User mode access to the PMU av_arm_(must be called in a priviledged mode)
void av_arm_disable_pmu_user_access(void);

#endif
// ------------------------------------------------------------
// End of v7_pmu.h
// ------------------------------------------------------------

