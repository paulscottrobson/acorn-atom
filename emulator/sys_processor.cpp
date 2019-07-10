// *******************************************************************************************************************************
// *******************************************************************************************************************************
//
//		Name:		sys_processor.c
//		Purpose:	Processor Emulation.
//		Created:	20th October 2015
//		Author:		Paul Robson (paul@robsons.org.uk)
//
// *******************************************************************************************************************************
// *******************************************************************************************************************************

#include <stdio.h>

#include "sys_processor.h"
#include "sys_debug_system.h"

// *******************************************************************************************************************************
//														   Timing
// *******************************************************************************************************************************

#define CYCLE_RATE 		(1*1000*1000)												// Cycles per second (0.96Mhz)
#define FRAME_RATE		(60)														// Frames per second (50 arbitrary)
#define CYCLES_PER_FRAME (CYCLE_RATE / FRAME_RATE)									// Cycles per frame (20,000)

// *******************************************************************************************************************************
//														CPU / Memory
// *******************************************************************************************************************************

static BYTE8 a,x,y,s;																// 6502 A,X,Y and Stack registers
static BYTE8 carryFlag,interruptDisableFlag,breakFlag,								// Values representing status reg
			 decimalFlag,overflowFlag,sValue,zValue;
static WORD16 pc;																	// Program Counter.
static BYTE8 ramMemory[RAMSIZE];													// Memory at $0000 upwards
static LONG32 cycles;																// Cycle Count.

// *******************************************************************************************************************************
//											 Memory and I/O read and write macros.
// *******************************************************************************************************************************

#define Read(a) 	_Read(a)														// Basic Read
#define Write(a,d)	_Write(a,d)														// Basic Write

#define Cycles(n) 	cycles += (n)													// Bump Cycles
#define Fetch() 	_Read(pc++)														// Fetch byte
#define FetchWord()	{ temp16 = Fetch();temp16 |= (Fetch() << 8); }					// Fetch word

#define ReadWord(a) (Read(a) | ((Read((a)+1) << 8)))								// Read 16 bit, Basic

//	These are optimised for page 0 and page 1
#define ReadWord01(a) ReadWord(a)													// Read 16 bit, page 0/1 
#define Read01(a) 	Read(a)															// Read 8 bit, page 0/1
#define Write01(a,d) Write(a,d)														// Write 8 bit, page 0/1

static inline BYTE8 _Read(WORD16 address);											// Need to be forward defined as 
static inline void _Write(WORD16 address,BYTE8 data);								// used in support functions.

#include "6502/__6502support.h"

// *******************************************************************************************************************************
//											   Read and Write Inline Functions
// *******************************************************************************************************************************

static BYTE8 currentKeyPending = 0;

static inline BYTE8 _Read(WORD16 address) {
	return ramMemory[address];							
}

static inline void _Write(WORD16 address,BYTE8 data) {
	ramMemory[address] = data;
}

// *******************************************************************************************************************************
//														Reset the CPU
// *******************************************************************************************************************************

#include "basic_rom.inc"
#include "kernel_rom.inc"

void CPUReset(void) {
	for (int i = 0;i < 4096;i++) ramMemory[0xF000+i] = kernel_rom[i];
	for (int i = 0;i < 4096;i++) ramMemory[0xC000+i] = basic_rom[i];
	resetProcessor();
}

// *******************************************************************************************************************************
//												Execute a single instruction
// *******************************************************************************************************************************

BYTE8 CPUExecuteInstruction(void) {
	BYTE8 opcode = Fetch();															// Fetch opcode.
	switch(opcode) {																// Execute it.
		#include "6502/__6502opcodes.h"
	}
	if (cycles < CYCLES_PER_FRAME) return 0;										// Not completed a frame.
	cycles = cycles - CYCLES_PER_FRAME;												// Adjust this frame rate.
	return FRAME_RATE;																// Return frame rate.
}


#ifdef INCLUDE_DEBUGGING_SUPPORT

// *******************************************************************************************************************************
//												Get time in ms (windows only)
// *******************************************************************************************************************************

#include "gfx.h"
LONG32 SYSMilliseconds(void) {
	return GFXTimer();
}

// *******************************************************************************************************************************
//		Execute chunk of code, to either of two break points or frame-out, return non-zero frame rate on frame, breakpoint 0
// *******************************************************************************************************************************

BYTE8 CPUExecute(WORD16 breakPoint1,WORD16 breakPoint2) { 
	BYTE8 next;
	do {
		BYTE8 r = CPUExecuteInstruction();											// Execute an instruction
		if (r != 0) return r; 														// Frame out.
		next = CPUReadMemory(pc);
	} while (pc != breakPoint1 && pc != breakPoint2 && next != 0x02);				// Stop on breakpoint or NOP
	return 0; 
}

// *******************************************************************************************************************************
//									Return address of breakpoint for step-over, or 0 if N/A
// *******************************************************************************************************************************

WORD16 CPUGetStepOverBreakpoint(void) {
	BYTE8 opcode = CPUReadMemory(pc);												// Current opcode.
	if (opcode == 0x20) return (pc+3) & 0xFFFF;										// Step over JSR.
	return 0;																		// Do a normal single step
}

// *******************************************************************************************************************************
//												Read/Write Memory
// *******************************************************************************************************************************

BYTE8 CPUReadMemory(WORD16 address) {
	return Read(address);
}

void CPUWriteMemory(WORD16 address,BYTE8 data) {
	Write(address,data);
}

void CPUEndRun(void) {
	FILE *f = fopen("memory.dump","wb");
	fwrite(ramMemory,1,RAMSIZE,f);
	fclose(f);
}

void CPUExit(void) {	
	GFXExit();
}

static void _CPULoadChunk(FILE *f,BYTE8* memory,int count) {
	while (count != 0) {
		int qty = (count > 4096) ? 4096 : count;
		fread(memory,1,qty,f);
		count = count - qty;
		memory = memory + qty;
	}
}
void CPULoadBinary(char *fileName) {
	FILE *f = fopen(fileName,"rb");
	_CPULoadChunk(f,ramMemory,RAMSIZE);
	fclose(f);
	resetProcessor();
}
#endif

// *******************************************************************************************************************************
//											Retrieve a snapshot of the processor
// *******************************************************************************************************************************

#ifdef INCLUDE_DEBUGGING_SUPPORT

static CPUSTATUS st;																	// Status area

CPUSTATUS *CPUGetStatus(void) {
	st.a = a;st.x = x;st.y = y;st.sp = s;st.pc = pc;
	st.carry = carryFlag;st.interruptDisable = interruptDisableFlag;st.zero = (zValue == 0);
	st.decimal = decimalFlag;st.brk = breakFlag;st.overflow = overflowFlag;
	st.sign = (sValue & 0x80) != 0;st.status = constructFlagRegister();
	st.cycles = cycles;
	return &st;
}

#endif