//************************************************************************
//**
//** Project......: Firmware USB AVR Si570 controler.
//**
//** Platform.....: ATtiny45
//**
//** Licence......: This software is freely available for non-commercial 
//**                use - i.e. for research and experimentation only!
//**                Copyright: (c) 2006 by OBJECTIVE DEVELOPMENT Software GmbH
//**                Based on ObDev's AVR USB driver by Christian Starkjohann
//**
//** Programmer...: F.W. Krom, PE0FKO
//**                I like to thank Francis Dupont, F6HSI for checking the
//**                algorithm and add some usefull comment!
//**                Thanks to Tom Baier DG8SAQ for the initial program.
//** 
//** Description..: Calculations for the Si570 chip and Si570 program algorithme.
//**                Changed faster and precise code to program the Si570 device.
//**
//** History......: V15.1 02/12/2008: First release of PE0FKO.
//**                Check the main.c file
//**
//**************************************************************************

#include "main.h"

#if INCLUDE_SI570

static	uint16_t	Si570_N;				// Total division (N1 * HS_DIV)
static	uint8_t		Si570_N1;				// The slow divider
static	uint8_t		Si570_HS_DIV;			// The high speed divider
#if INCLUDE_SMOOTH
		uint32_t	FreqSmoothTune;			// The smooth tune center frequency
#endif
static	void		Si570WriteSmallChange(void);
static	void		Si570WriteLargeChange(void);

#include "CalcVFO.c"						// Include code is small size

// Cost: 140us
// This function only works for the "C" & "B" grade of the Si570 chip.
// It will not check the frequency gaps for the "A" grade chip!!!
static uint8_t
Si570CalcDivider(uint32_t freq)
{
	// Register finding the lowest DCO frequenty
	uint8_t		xHS_DIV;
	sint16_t	xN1;
	uint16_t	xN;

	// Registers to save the found dividers
	uint8_t		sHS_DIV	= 0;
	uint8_t		sN1		= 0;
	uint16_t	sN		= 11*128;		// Total dividing
	uint16_t	N0;						// Total divider needed (N1 * HS_DIV)
	sint32_t	Freq;

	Freq.dw = freq;

	// Find the total division needed.
	// It is always one to low (not in the case reminder is zero, reminder not used here).
	// 16.0 bits = 13.3 bits / ( 11.5 bits >> 2)
#if INCLUDE_SI570_GRADE
	N0 = (R.Si570DCOMin * (uint16_t)(_2(3))) / (Freq.w1.w >> 2);
#else
	N0 = (DCO_MIN * _2(3)) / (Freq.w1.w >> 2);
#endif

	for(xHS_DIV = 11; xHS_DIV > 3; --xHS_DIV)
	{
		// Skip the unavailable divider's
		if (xHS_DIV == 8 || xHS_DIV == 10)
			continue;

		// Calculate the needed low speed divider
		xN1.w = N0 / xHS_DIV + 1;

		if (xN1.w > 128)
			continue;

		// Skip the unavailable N1 divider's
		if (xN1.b0 != 1 && (xN1.b0 & 1) == 1)
			xN1.b0 += 1;

#if INCLUDE_SI570_GRADE
		if (R.Si570Grade == CHIP_SI570_A)
		{
			// No divider restrictions!
		}
		else
		if (R.Si570Grade == CHIP_SI570_B)
		{
			if ((xN1.b0 == 1 && xHS_DIV == 4)
			||	(xN1.b0 == 1 && xHS_DIV == 5))
			{
				continue;
			}
		}
		else
		if (R.Si570Grade == CHIP_SI570_C)
		{
			if ((xN1.b0 == 1 && xHS_DIV == 4)
			||	(xN1.b0 == 1 && xHS_DIV == 5)
			||	(xN1.b0 == 1 && xHS_DIV == 6)
			||	(xN1.b0 == 1 && xHS_DIV == 7)
			||	(xN1.b0 == 1 && xHS_DIV == 11)
			||	(xN1.b0 == 2 && xHS_DIV == 4)
			||	(xN1.b0 == 2 && xHS_DIV == 5)
			||	(xN1.b0 == 2 && xHS_DIV == 6)
			||	(xN1.b0 == 2 && xHS_DIV == 7)
			||	(xN1.b0 == 2 && xHS_DIV == 9)
			||	(xN1.b0 == 4 && xHS_DIV == 4))
			{
				continue;
			}
		} 
		else
		if (R.Si570Grade == CHIP_SI570_D)
		{
			if ((xN1.b0 == 1 && xHS_DIV == 4)
			||	(xN1.b0 == 1 && xHS_DIV == 5)
			||	(xN1.b0 == 1 && xHS_DIV == 6)
			||	(xN1.b0 == 1 && xHS_DIV == 7)
			||	(xN1.b0 == 1 && xHS_DIV == 11)
			||	(xN1.b0 == 2 && xHS_DIV == 4)
			||	(xN1.b0 == 2 && xHS_DIV == 5)
			||	(xN1.b0 == 2 && xHS_DIV == 6)
			||	(xN1.b0 == 2 && xHS_DIV == 7)
			||	(xN1.b0 == 2 && xHS_DIV == 9))
			// Removing the 4*4 is out of the spec of the C grade chip, it may work!
//			||	(xN1.b0 == 4 && xHS_DIV == 4))
			{
				continue;
			}
		}
		else
		{
		}
#endif


		xN = xHS_DIV * xN1.b0;
		if (sN > xN)
		{
			sN		= xN;
			sN1		= xN1.b0;
			sHS_DIV	= xHS_DIV;
		}
	}

	if (sHS_DIV == 0)
		return false;

	Si570_N      = sN;
	Si570_N1     = sN1;
	Si570_HS_DIV = sHS_DIV;

	return true;
}

// Cost: 140us
// frequency [MHz] * 2^21
static 
uint8_t
Si570CalcRFREQ(uint32_t freq)
{
	uint8_t		cnt;
	sint32_t	RFREQ;
	uint8_t		RFREQ_b4;
	uint32_t	RR;						// Division remainder
	uint8_t		sN1;

	// Convert divider ratio to SI570 register value
	sN1 = Si570_N1 - 1;
	Si570_Data.N1      = sN1 >> 2;
	Si570_Data.HS_DIV  = Si570_HS_DIV - 4;

	//============================================================================
	// RFREQ = freq * sN * 8 / Xtal
	//============================================================================
	// freq = F * 2^21 => 11.21 bits
	// xtal = F * 2^24 =>  8.24 bits

	// 1- RFREQ:b4 =  Si570_N * freq
	//------------------------------

	//----------------------------------------------------------------------------
	// Product_48 = Multiplicand_16 x Multiplier_32
	//----------------------------------------------------------------------------
	// Multiplicand_16:  N_MSB   N_LSB
	// Multiplier_32  :                  b3      b2      b1      b0
	// Product_48     :  r0      b4      b3      b2      b1      b0
	//                  <--- high ----><---------- low ------------->

	cnt = 32+1;                      // Init loop counter
	asm (
	"clr __tmp_reg__     \n\t"     // Clear Product high bytes  & carry
	"sub %1,%1           \n\t"     // (C = 0)

"L_A_%=:                 \n\t"     // Repeat

	"brcc L_B_%=         \n\t"     //   If(Cy -bit 0 of Multiplier- is set)

	"add %1,%A2          \n\t"     //   Then  add Multiplicand to Product high bytes
	"adc __tmp_reg__,%B2 \n\t"

"L_B_%=:                 \n\t"     //   End If

	                              //   Shift right Product
	"ror __tmp_reg__     \n\t"     //   Cy -> r0
	"ror %A1             \n\t"     //            ->b4
	"ror %D0             \n\t"     //                 -> b3
	"ror %C0             \n\t"     //                       -> b2
	"ror %B0             \n\t"     //                             -> b1
	"ror %A0             \n\t"     //                                   -> b0 -> Cy

	"dec %3              \n\t"     // Until(--cnt == 0)
	"brne L_A_%=         \n\t"

	// Output operand list
	//--------------------
	: "=r" (RFREQ.dw)               // %0 -> Multiplier_32/Product b0,b1,b2,b3
	, "=r" (RFREQ_b4)               // %1 -> Product b4
	
	// Input operand list
	//-------------------
	: "r" (Si570_N)                 // %2 -> Multiplicand_16
	, "r" (cnt)                     // %3 -> Loop_Counter
	, "0" (freq)

//	: "r0"                          // r0 -> Tempory register
	);

	// Check if DCO is lower than the Si570 max specied.
	// The low 3 bit's are not used, so the error is 8MHz
	// DCO = Freq * sN (calculated above)
	// RFREQ is [19.21]bits
	sint16_t DCO;
	DCO.b0 = RFREQ.w1.b1;
	DCO.b1 = RFREQ_b4;
#if INCLUDE_SI570_GRADE
	if (DCO.w > ((R.Si570DCOMax+4)/8))
		return 0;
#else
	if (DCO.w > ((DCO_MAX+4)/8))
		return 0;
#endif

	// 2- RFREQ:b4 = RFREQ:b4 * 8 / FreqXtal
	//---------------------------------------------
	
	//---------------------------------------------------------------------------
	// Quotient_40 = Dividend_40 / Divisor_32
	//---------------------------------------------------------------------------
	// Dividend_40: RFREQ     b4      b3      b2      b1      b0
	// Divisor_32 : FreqXtal
	// Quotient_40: RFREQ     b4      b3      b2      b1      b0
	//---------------------------------------------------------------------------

	RR = 0;							// Clear Remainder_40
	cnt = 40+1+28+3;				// Init Loop_Counter
									// (28 = 12.28 bits, 3 = * 8)
	asm (
	"clc                 \n\t"		// Partial_result = carry = 0
	
"L_A_%=:                 \n\t"		// Repeat
	"rol %0              \n\t"		//   Put last Partial_result in Quotient_40
	"rol %1              \n\t"		//   and shift left Dividend_40 ...
	"rol %2              \n\t"
	"rol %3              \n\t"
	"rol %4              \n\t"

	"rol %A6             \n\t"		//                      ... into Remainder_40
	"rol %B6             \n\t"
	"rol %C6             \n\t"
	"rol %D6             \n\t"

	"sub %A6,%A7         \n\t"		//   Remainder =  Remainder - Divisor
	"sbc %B6,%B7         \n\t"
	"sbc %C6,%C7         \n\t"
	"sbc %D6,%D7         \n\t"

	"brcc L_B_%=         \n\t"		//   If result negative
									//   Then
	"add %A6,%A7         \n\t"		//     Restore Remainder
	"adc %B6,%B7         \n\t"
	"adc %C6,%C7         \n\t"
	"adc %D6,%D7         \n\t"

	"clc                 \n\t"		//     Partial_result = 0
	"rjmp L_C_%=         \n\t"

"L_B_%=:                 \n\t"		//   Else
	"sec                 \n\t"		//     Partial_result = 1

"L_C_%=:                 \n\t"		//   End If
	"dec %5              \n\t"		// Until(--cnt == 0)
	"brne L_A_%=         \n\t"

	"adc %0,__zero_reg__ \n\t"		// Round by the last bit of RFREQ
	"adc %1,__zero_reg__ \n\t"
	"adc %2,__zero_reg__ \n\t"
	"adc %3,__zero_reg__ \n\t"
	"adc %4,__zero_reg__ \n\t"

"L_X_%=:               \n\t"

	// Output operand list
	//--------------------
	: "=r" (Si570_Data.RFREQ.w1.b1) // %0 -> Dividend_40
	, "=r" (Si570_Data.RFREQ.w1.b0) // %1        "
	, "=r" (Si570_Data.RFREQ.w0.b1) // %2        "
	, "=r" (Si570_Data.RFREQ.w0.b0) // %3        "     LSB
	, "=r" (RFREQ_b4)               // %4        "     MSB
	
	// Input operand list
	//-------------------
	: "r" (cnt)                     // %5 -> Loop_Counter
	, "r" (RR)                      // %6 -> Remainder_40
	, "r" (R.FreqXtal)              // %7 -> Divisor_32
	, "0" (RFREQ.w0.b0)
	, "1" (RFREQ.w0.b1)
	, "2" (RFREQ.w1.b0)
	, "3" (RFREQ.w1.b1)
	, "4" (RFREQ_b4)
	);

	// Si570_Data.RFREQ_b4 will be sent to register_8 in the Si570
	// register_8 :  76543210
	//               ||^^^^^^------< RFREQ[37:32]
	//               ^^------------< N1[1:0]
	Si570_Data.RFREQ_b4  = RFREQ_b4;
	Si570_Data.RFREQ_b4 |= (sN1 & 0x03) << 6;

	return 1;
}


#if INCLUDE_SMOOTH

static uint8_t
Si570_Small_Change(uint32_t current_Frequency)
{
	uint32_t delta_F, delta_F_MAX;
	sint32_t previous_Frequency;

	// Get previous_Frequency   -> [11.21]
	previous_Frequency.dw = FreqSmoothTune;

	// Delta_F (MHz) = |current_Frequency - previous_Frequency|  -> [11.21]
	delta_F = current_Frequency - previous_Frequency.dw;
	if (delta_F >= _2(31)) delta_F = 0 - delta_F;

	// Delta_F (Hz) = (Delta_F (MHz) * 1_000_000) >> 16 not possible, overflow
	// replaced by:
	// Delta_F (Hz) = (Delta_F (MHz) * (1_000_000 >> 16)
	//              = Delta_F (MHz) * 15  (instead of 15.258xxxx)
	// Error        = (15 - 15.258) / 15.258 = 0.0169 < 1.7%

	delta_F = delta_F * 15;          // [27.5] = [11.21] * [16.0]

	// Compute delta_F_MAX (Hz)= previous_Frequency(MHz) * 3500 ppm
	delta_F_MAX = (uint32_t)previous_Frequency.w1.w * R.SmoothTunePPM;
	//   [27.5] =                          [11.5] * [16.0]

	// return TRUE if output changes less than �3500 ppm from the previous_Frequency
	return (delta_F <= delta_F_MAX) ? true : false;
}

#endif

#if INCLUDE_IBPF

static uint8_t
GetFreqBand(uint32_t freq)
{
	uint8_t n;
	sint32_t Freq;

	Freq.dw = freq;

	for(n=0; n < MAX_BAND-1; ++n)
		if (Freq.w1.w < R.FilterCrossOver[n].w)
			return n;

	return MAX_BAND-1;
}

void
SetFilter(uint8_t filter)
{
	if (FilterCrossOverOn)
	{
		bit_1(IO_DDR, IO_P1);
		bit_1(IO_DDR, IO_P2);

		if (filter & 0x01)
			bit_1(IO_PORT, IO_P1);
		else
			bit_0(IO_PORT, IO_P1);

		if (filter & 0x02)
			bit_1(IO_PORT, IO_P2);
		else
			bit_0(IO_PORT, IO_P2);
	}
}

#endif

void
SetFreq(uint32_t freq)		// frequency [MHz] * 2^21
{
	R.Freq = freq;			// Save the asked freq

#if INCLUDE_IBPF

	uint8_t band = GetFreqBand(freq);

	freq = CalcFreqMulAdd(freq, R.BandSub[band], R.BandMul[band]);

	SetFilter(R.Band2Filter[band]);

#endif

//#ifdef INCLUDE_ABPF	<<-- Bug in V15.12
#if INCLUDE_ABPF
	if (FilterCrossOverOn)
	{
		sint32_t Freq;
		Freq.dw = R.Freq;			// Freq.w1 is 11.5bits

		bit_1(IO_DDR, IO_P1);
		bit_1(IO_DDR, IO_P2);

		if (Freq.w1.w < R.FilterCrossOver[0].w)
		{
			bit_0(IO_PORT, IO_P1);
			bit_0(IO_PORT, IO_P2);
		}
		else 
		if (Freq.w1.w < R.FilterCrossOver[1].w)
		{
			bit_1(IO_PORT, IO_P1);
			bit_0(IO_PORT, IO_P2);
		}
		else 
		if (Freq.w1.w < R.FilterCrossOver[2].w)
		{
			bit_0(IO_PORT, IO_P1);
			bit_1(IO_PORT, IO_P2);
		}
		else 
		{
			bit_1(IO_PORT, IO_P1);
			bit_1(IO_PORT, IO_P2);
		}
	}
#endif

#if INCLUDE_FREQ_SM

	freq = CalcFreqMulAdd(freq, R.FreqSub, R.FreqMul);

#endif

#if INCLUDE_SMOOTH

	if ((R.SmoothTunePPM != 0) && Si570_Small_Change(freq))
	{
		Si570CalcRFREQ(freq);
		Si570WriteSmallChange();
	}
	else
	{
		if (!Si570CalcDivider(freq) || !Si570CalcRFREQ(freq))
			return;

		FreqSmoothTune = freq;
		Si570WriteLargeChange();
	}

#else

	if (!Si570CalcDivider(freq) || !Si570CalcRFREQ(freq))
		return;

	Si570WriteLargeChange();

#endif
}

#if INCLUDE_SI570_GRADE

// Check Si570 old/new 'signature' 07h, C2h, C0h, 00h, 00h, 00h
static uint8_t
Check_Signature()
{
	static PROGMEM uint8_t signature[] = { 0x07, 0xC2, 0xC0, 0x00, 0x00, 0x00 };
	uint8_t i;

	if (!Si570ReadRFREQ(RFREQ_13_INDEX))
		return true;

	for(i = 0; i < sizeof(signature); ++i)
		if (pgm_read_byte(&signature[i]) != Si570_Data.bData[i])
			break;

	return i == 6;	//sizeof(signature);
}

static void
Auto_index_detect_RFREQ(void)
{
	if ((R.Si570RFREQIndex & RFREQ_INDEX) == RFREQ_AUTO_INDEX)
	{
		// First RECALL the Si570 to default settings.
		Si570CmdReg(135, 0x01);
		_delay_us(100.0);

		// Check if signature found, then it is a old or new 50/20ppm chip
		// If not found it must be a *new* Si570 7ppm chip!
		if (Check_Signature())
		{
			R.Si570RFREQIndex &= RFREQ_FREEZE;
			R.Si570RFREQIndex |= RFREQ_7_INDEX;
		}
		else
		{
			R.Si570RFREQIndex &= RFREQ_FREEZE;
			R.Si570RFREQIndex |= RFREQ_13_INDEX;
		}
	}
}
#endif

void
DeviceInit(void)
{
	// Check if Si570 is online and intialize if nessesary
	// SCL Low is now power on the SI570 chip in the Softrock V9
	if ((I2C_PIN & _BV(BIT_SCL)) != 0)
	{
		if (SI570_OffLine)
		{
#if INCLUDE_SMOOTH
			FreqSmoothTune = 0;				// Next SetFreq call no smoodtune
#endif
#if INCLUDE_SI570_GRADE
			Auto_index_detect_RFREQ();
#endif
			SetFreq(R.Freq);

			SI570_OffLine = I2CErrors;
		}
	}
	else 
	{
		SI570_OffLine = true;
	}
}

static uint8_t
Si570CmdStart(uint8_t cmd)
{
	I2CSendStart();
	I2CSendByte((R.ChipCrtlData<<1)|0);	// send device address 
	if (I2CErrors == 0)
	{
		I2CSendByte(cmd);				// send Byte Command
		return true;
	}
	return false;
}

void
Si570CmdReg(uint8_t reg, uint8_t data)
{
	if (Si570CmdStart(reg))
	{
		I2CSendByte(data);
	}
	I2CSendStop();
}

// write all registers in one block from Si570_Data
static void
Si570WriteRFREQ(void)
{
	if (Si570CmdStart(R.Si570RFREQIndex & RFREQ_INDEX))	// send Byte address 7/13
	{
		uint8_t i;
		for (i=0;i<6;i++)				// all 6 registers
			I2CSendByte(Si570_Data.bData[i]);// send data 
	}
	I2CSendStop();
}

// read all registers in one block to Si570_Data
uint8_t
Si570ReadRFREQ(uint8_t index)
{
	if (Si570CmdStart(index & RFREQ_INDEX))	// send reg address 7 or 13
	{
		uint8_t i;
		I2CSendStart();
		I2CSendByte((R.ChipCrtlData<<1)|1);
		for (i=0; i<5; i++)
		{
			Si570_Data.bData[i] = I2CReceiveByte();
			I2CSend0();					// 0 more bytes to follow
		}
		Si570_Data.bData[5] = I2CReceiveByte();
		I2CSend1();						// 1 Last byte
	}
	I2CSendStop(); 

	return I2CErrors ? 0 : sizeof(Si570_t);
}

static void
Si570WriteSmallChange(void)
{
	if (R.Si570RFREQIndex & RFREQ_FREEZE)
	{
		// Prevents interim frequency changes when writing RFREQ registers.
		Si570CmdReg(135, 1<<5);		// Freeze M
		if (I2CErrors == 0)
		{
			Si570WriteRFREQ();
			Si570CmdReg(135, 0<<5);	// unFreeze M
		}
	}
	else
	{
		Si570WriteRFREQ();
	}
}

static void
Si570WriteLargeChange(void)
{
	Si570CmdReg(137, 1<<4);			// Freeze NCO
	if (I2CErrors == 0)
	{
		Si570WriteRFREQ();
		Si570CmdReg(137, 0<<4);		// unFreeze NCO
		Si570CmdReg(135, 1<<6);		// NewFreq set (auto clear)
	}
}

#endif

