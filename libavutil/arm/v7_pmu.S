/*------------------------------------------------------------
Performance Monitor Block
------------------------------------------------------------*/
    .arm  @ Make sure we are in ARM mode.
    .text
    .align 2
    .global getPMN @ export this function for the linker

/*  Returns the number of progammable counters uint32_t getPMN(void) */

getPMN:
  MRC     p15, 0, r0, c9, c12, 0 /* Read PMNC Register	*/
  MOV     r0, r0, LSR #11        /* Shift N field down to bit 0	*/
  AND     r0, r0, #0x1F          /* Mask to leave just the 5 N bits	*/
  BX      lr



    .global pmn_config @ export this function for the linker
  /* Sets the event for a programmable counter to record	*/
  /* void pmn_config(unsigned counter, uint32_t event)	*/
  /* counter = r0 = Which counter to program  (e.g. 0 for PMN0, 1 for PM
  /* event   = r1 = The event code	*/
pmn_config:
  AND     r0, r0, #0x1F          /* Mask to leave only bits 4:0	*/
  MCR     p15, 0, r0, c9, c12, 5 /* Write PMNXSEL Register	*/
  MCR     p15, 0, r1, c9, c13, 1 /* Write EVTSELx Register	*/
  BX      lr



    .global ccnt_divider @ export this function for the linker
  /* Enables/disables the divider (1/64) on CCNT	*/
  /* void ccnt_divider(int divider)	*/
  /* divider = r0 = If 0 disable divider, else enable dvider	*/
ccnt_divider:
  MRC     p15, 0, r1, c9, c12, 0  /* Read PMNC	*/

  CMP     r0, #0x0                /* IF (r0 == 0)	*/
  BICEQ   r1, r1, #0x08           /* THEN: Clear the D bit (disables the
  ORRNE   r1, r1, #0x08           /* ELSE: Set the D bit (enables the di

  MCR     p15, 0, r1, c9, c12, 0  /* Write PMNC	*/
  BX      lr


  /* ---------------------------------------------------------------	*/
  /* Enable/Disable	*/
  /* ---------------------------------------------------------------	*/

    .global enable_pmu @ export this function for the linker
  /* Global PMU enable	*/
  /* void enable_pmu(void)	*/
enable_pmu:
  MRC     p15, 0, r0, c9, c12, 0  /* Read PMNC	*/
  ORR     r0, r0, #0x01           /* Set E bit	*/
  MCR     p15, 0, r0, c9, c12, 0  /* Write PMNC	*/
  BX      lr



    .global disable_pmu @ export this function for the linker
  /* Global PMU disable	*/
  /* void disable_pmu(void)	*/
disable_pmu:
  MRC     p15, 0, r0, c9, c12, 0  /* Read PMNC	*/
  BIC     r0, r0, #0x01           /* Clear E bit	*/
  MCR     p15, 0, r0, c9, c12, 0  /* Write PMNC	*/
  BX      lr



    .global enable_ccnt @ export this function for the linker
  /* Enable the CCNT	*/
  /* void enable_ccnt(void)	*/
enable_ccnt:
  MOV     r0, #0x80000000         /* Set C bit	*/
  MCR     p15, 0, r0, c9, c12, 1  /* Write CNTENS Register	*/
  BX      lr



    .global disable_ccnt @ export this function for the linker
  /* Disable the CCNT	*/
  /* void disable_ccnt(void)	*/
disable_ccnt:
  MOV     r0, #0x80000000         /* Clear C bit	*/
  MCR     p15, 0, r0, c9, c12, 2  /* Write CNTENC Register	*/
  BX      lr



    .global enable_pmn @ export this function for the linker
  /* Enable PMN{n}	*/
  /* void enable_pmn(uint32_t counter)	*/
  /* counter = r0 = The counter to enable (e.g. 0 for PMN0, 1 for PMN1)
enable_pmn:
  MOV     r1, #0x1                /* Use arg (r0) to set which counter t
  MOV     r1, r1, LSL r0

  MCR     p15, 0, r1, c9, c12, 1  /* Write CNTENS Register	*/
  BX      lr



    .global disable_pmn @ export this function for the linker
  /* Enable PMN{n}	*/
  /* void disable_pmn(uint32_t counter)	*/
  /* counter = r0 = The counter to enable (e.g. 0 for PMN0, 1 for PMN1)
disable_pmn:
  MOV     r1, #0x1                /* Use arg (r0) to set which counter t
  MOV     r1, r1, LSL r0

  MCR     p15, 0, r1, c9, c12, 1  /* Write CNTENS Register	*/
  BX      lr



    .global enable_pmu_user_access @ export this function for the linker
  /* Enables User mode access to the PMU (must be called in a priviledge
  /* void enable_pmu_user_access(void)	*/
enable_pmu_user_access:
  MRC     p15, 0, r0, c9, c14, 0  /* Read PMUSERENR Register	*/
  ORR     r0, r0, #0x01           /* Set EN bit (bit 0)	*/
  MCR     p15, 0, r0, c9, c14, 0  /* Write PMUSERENR Register	*/
  BX      lr



    .global disable_pmu_user_access @ export this function for the linke
  /* Disables User mode access to the PMU (must be called in a priviledg
  /* void disable_pmu_user_access(void)	*/
disable_pmu_user_access:
  MRC     p15, 0, r0, c9, c14, 0  /* Read PMUSERENR Register	*/
  BIC     r0, r0, #0x01           /* Clear EN bit (bit 0)	*/
  MCR     p15, 0, r0, c9, c14, 0  /* Write PMUSERENR Register	*/
  BX      lr


  /* ---------------------------------------------------------------	*/
  /* Counter read registers	*/
  /* ---------------------------------------------------------------	*/

    .global read_ccnt @ export this function for the linker
  /* Returns the value of CCNT	*/
  /* uint32_t read_ccnt(void)	*/
read_ccnt:
  MRC     p15, 0, r0, c9, c13, 0 /* Read CCNT Register	*/
  BX      lr


    .global read_pmn @ export this function for the linker
  /* Returns the value of PMN{n}	*/
  /* uint32_t read_pmn(uint32_t counter)	*/
  /* counter = r0 =  The counter to read (e.g. 0 for PMN0, 1 for PMN1)	*
read_pmn:
  AND     r0, r0, #0x1F          /* Mask to leave only bits 4:0	*/
  MCR     p15, 0, r0, c9, c12, 5 /* Write PMNXSEL Register	*/
  MRC     p15, 0, r0, c9, c13, 2 /* Read current PMNx Register	*/
  BX      lr


  /* ---------------------------------------------------------------	*/
  /* Software Increment	*/
  /* ---------------------------------------------------------------	*/

    .global pmu_software_increment @ export this function for the linker
	/* Writes to software increment register	*/
	/* void pmu_software_increment(uint32_t counter)	*/
	/* counter = r0 =  The counter to increment (e.g. 0 for PMN0, 1 for PMN
pmu_software_increment:
  MOV     r1, #0x01
  MOV			r1, r1, LSL r0
  MCR     p15, 0, r1, c9, c12, 4 /* Write SWINCR Register	*/
  BX      lr

  /* ---------------------------------------------------------------	*/
  /* Overflow & Interrupt Generation	*/
  /* ---------------------------------------------------------------	*/

    .global read_flags @ export this function for the linker
  /* Returns the value of the overflow flags	*/
  /* uint32_t read_flags(void)	*/
read_flags:
  MRC     p15, 0, r0, c9, c12, 3 /* Read FLAG Register	*/
  BX      lr


    .global write_flags @ export this function for the linker
  /* Writes the overflow flags	*/
  /* void write_flags(uint32_t flags)	*/
write_flags:
  MCR     p15, 0, r0, c9, c12, 3 /* Write FLAG Register	*/
  BX      lr


    .global enable_ccnt_irq @ export this function for the linker
  /* Enables interrupt generation on overflow of the CCNT	*/
  /* void enable_ccnt_irq(void)	*/
enable_ccnt_irq:
  MOV     r0, #0x80000000
  MCR     p15, 0, r0, c9, c14, 1  /* Write INTENS Register	*/
  BX      lr

    .global disable_ccnt_irq @ export this function for the linker
  /* Disables interrupt generation on overflow of the CCNT	*/
  /* void disable_ccnt_irq(void)	*/
disable_ccnt_irq:
  MOV     r0, #0x80000000
  MCR     p15, 0, r0, c9, c14, 2   /* Write INTENC Register	*/
  BX      lr


    .global enable_pmn_irq @ export this function for the linker
  /* Enables interrupt generation on overflow of PMN{x}	*/
  /* void enable_pmn_irq(uint32_t counter)	*/
  /* counter = r0 =  The counter to enable the interrupt for (e.g. 0 for
enable_pmn_irq:
  MOV     r1, #0x1                 /* Use arg (r0) to set which counter
  MOV     r0, r1, LSL r0
  MCR     p15, 0, r0, c9, c14, 1   /* Write INTENS Register	*/
  BX      lr

    .global disable_pmn_irq @ export this function for the linker
  /* Disables interrupt generation on overflow of PMN{x}	*/
  /* void disable_pmn_irq(uint32_t counter)	*/
  /* counter = r0 =  The counter to disable the interrupt for (e.g. 0 fo
disable_pmn_irq:
  MOV     r1, #0x1                /* Use arg (r0) to set which counter t
  MOV     r0, r1, LSL r0
  MCR     p15, 0, r0, c9, c14, 2  /* Write INTENC Register	*/
  BX      lr

  /* ---------------------------------------------------------------	*/
  /* Reset Functions	*/
  /* ---------------------------------------------------------------	*/

    .global reset_pmn @ export this function for the linker
  /* Resets the programmable counters	*/
  /* void reset_pmn(void)	*/
reset_pmn:
  MRC     p15, 0, r0, c9, c12, 0  /* Read PMNC	*/
  ORR     r0, r0, #0x02           /* Set P bit (Event Counter Reset)	*/
  MCR     p15, 0, r0, c9, c12, 0  /* Write PMNC	*/
  BX      lr


	.global reset_ccnt @ export this function for the linker
  /* Resets the CCNT	*/
  /* void reset_ccnt(void)	*/
reset_ccnt:
  MRC     p15, 0, r0, c9, c12, 0  /* Read PMNC	*/
  ORR     r0, r0, #0x04           /* Set C bit (Event Counter Reset)	*/
  MCR     p15, 0, r0, c9, c12, 0  /* Write PMNC	*/
  BX      lr


    .end @end of code, this line is optional.
/* ------------------------------------------------------------	*/
/* End of v7_pmu.s	*/
/* ------------------------------------------------------------	*/


