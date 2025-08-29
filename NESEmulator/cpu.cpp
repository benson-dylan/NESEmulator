#include "cpu.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdlib>

// Control Flags
const uint8_t C_FLAG = 0x01; // Carry       - bit 0
const uint8_t Z_FLAG = 0x02; // Zero        - bit 1
const uint8_t I_FLAG = 0x04; // Interrupt   - bit 2
const uint8_t D_FLAG = 0x08; // Decimal     - bit 3
const uint8_t B_FLAG = 0x10; // Break       - bit 4
const uint8_t U_FLAG = 0x20; // Unused      - bit 5 (usually set to 1)
const uint8_t V_FLAG = 0x40; // Overflow    - bit 6
const uint8_t N_FLAG = 0x80; // Negative    - bit 7


CPU::CPU(Memory* memory, NEW_PPU* ppu)
{
	this->memory = memory;
	this->ppu = ppu;
	reset();
}

void CPU::reset()
{
	A = X = Y = 0;
	SR = 0x24; // Set unused flag, interrupt disable, and break flag
	SP = 0XFD;
	// Starting address for PC is typically stored in the last 2 addresses of memory
	PC = (static_cast<uint16_t>(getMemory(0xFFFD)) << 8) | static_cast<uint16_t>(getMemory(0xFFFC));
	//PC = 0x8000; // FOR TESTING
	cycles = 0;
	ram.fill(0);
	opName = "JMP";
}

void CPU::step()
{
	if (ppu->isDMATriggered())
	{
		uint16_t dmaAddr = static_cast<uint16_t>(ppu->getDMAPage()) << 8;
		ppu->writeRegister(0x3, 0x00);
		for (int i = 0; i < 256; ++i)
		{
			ppu->writeRegister(0x4, getMemory(dmaAddr + i));
		}
		cycles += 513 + (cycles % 2);
		ppu->clearDMA();
	}

	if (irqPending && !(SR & I_FLAG))
	{
		handleIRQ();
		irqPending = false;
	}

	uint64_t startCycles = cycles;
	//logState(opName);
	uint8_t opcode = getMemory(PC++);
	execute(opcode);
	opName = getOpName(opcode);
	
	uint64_t deltaCycles = cycles - startCycles;
	ppu->step(deltaCycles);
}

void CPU::handleNMI()
{
	//printf("NMI entered, PC=%04X\n", PC);
	push((PC >> 8) & 0xFF);
	push(PC & 0xFF);
	push(SR & ~B_FLAG | U_FLAG);
	SR |= I_FLAG;
	PC = getMemory(0xFFFA) | (static_cast<uint16_t>(getMemory(0xFFFB)) << 8);
	cycles += 7;
	ppu->clearNMI();
}

void CPU::handleIRQ()
{
	push((PC >> 8) & 0xFF); 
	push(PC & 0xFF); 
	push(SR | B_FLAG | U_FLAG); 
	SR |= I_FLAG; 
	PC = getMemory(0xFFFE) | (static_cast<uint16_t>(getMemory(0xFFFF)) << 8); 
	cycles += 7;
}

void CPU::execute(uint8_t op)
{
	switch (op) 
	{
	case 0xA9: // LDA Immediate
	case 0xA5: // LDA Zero Page
	case 0xB5:
	case 0xAD: // LDA Absolute
	case 0xBD:
	case 0xB9:
	case 0xA1:
	case 0xB1:
		LDA(op);
		break;
	case 0xA2:
	case 0xA6:
	case 0xB6:
	case 0xAE:
	case 0xBE:
		LDX(op);
		break;
	case 0xA0:
	case 0xA4:
	case 0xB4:
	case 0xAC:
	case 0xBC:
		LDY(op);
		break;
	case 0x85:
	case 0x95:
	case 0x8D:
	case 0x9D:
	case 0x99:
	case 0x81:
	case 0x91:
		STA(op);
		break;
	case 0x86:
	case 0x96:
	case 0x8E:
		STX(op);
		break;
	case 0x84:
	case 0x94:
	case 0x8C:
		STY(op);
		break;
	case 0xAA:
		TAX();
		break;
	case 0xA8:
		TAY();
		break;
	case 0x8A:
		TXA();
		break;
	case 0x98:
		TYA();
		break;
	case 0xBA:
		TSX();
		break;
	case 0x9A:
		TXS();
		break;
	case 0x48:
		PHA();
		break;
	case 0x08:
		PHP();
		break;
	case 0x68:
		PLA();
		break;
	case 0x28:
		PLP();
		break;
	case 0x29:
	case 0x25:
	case 0x35:
	case 0x2D:
	case 0x3D:
	case 0X39:
	case 0x21:
	case 0x31:
		AND(op);
		break;
	case 0x49:
	case 0x45:
	case 0x55:
	case 0x4D:
	case 0x5D:
	case 0x59:
	case 0x41:
	case 0x51:
		EOR(op);
		break;
	case 0x09:
	case 0x05:
	case 0x15:
	case 0x0D:
	case 0x1D:
	case 0x19:
	case 0x01:
	case 0x11:
		ORA(op);
		break;
	case 0x24:
	case 0x2C:
		BIT(op);
		break;
	case 0x69:
	case 0x65:
	case 0x75:
	case 0x6D:
	case 0x7D:
	case 0x79:
	case 0x61:
	case 0x71:
		ADC(op);
		break;
	case 0xE9:
	case 0xE5:
	case 0xF5:
	case 0xED:
	case 0xFD:
	case 0xF9:
	case 0xE1:
	case 0xF1:
	case 0xEB:
		SBC(op);
		break;
	case 0xC9:
	case 0xC5:
	case 0xD5:
	case 0xCD:
	case 0xDD:
	case 0xD9:
	case 0xC1:
	case 0xD1:
		CMP(op);
		break;
	case 0xE0:
	case 0xE4:
	case 0xEC:
		CPX(op);
		break;
	case 0xC0:
	case 0xC4:
	case 0xCC:
		CPY(op);
		break;
	case 0xE6:
	case 0xF6:
	case 0xEE:
	case 0xFE:
		INC(op);
		break;
	case 0xE8:
		INX();
		break;
	case 0xC8:
		INY();
		break;
	case 0xC6:
	case 0xD6:
	case 0xCE:
	case 0xDE:
		DEC(op);
		break;
	case 0xCA:
		DEX();
		break;
	case 0x88:
		DEY();
		break;
	case 0x0A:
	case 0x06:
	case 0x16:
	case 0x0E:
	case 0x1E:
		ASL(op);
		break;
	case 0x4A:
	case 0x46:
	case 0x56:
	case 0x4E:
	case 0x5E:
		LSR(op);
		break;
	case 0x2A:
	case 0x26:
	case 0x36:
	case 0x2E:
	case 0x3E:
		ROL(op);
		break;
	case 0x6A:
	case 0x66:
	case 0x76:
	case 0x6E:
	case 0x7E:
		ROR(op);
		break;
	case 0x4C:
	case 0x6C:
		JMP(op);
		break;
	case 0x20:
		JSR();
		break;
	case 0x60:
		RTS();
		break;
	case 0x90:
		BCC();
		break;
	case 0xB0:
		BCS();
		break;
	case 0xF0:
		BEQ();
		break;
	case 0x30:
		BMI();
		break;
	case 0xD0:
		BNE();
		break;
	case 0x10:
		BPL();
		break;
	case 0x50:
		BVC();
		break;
	case 0x70:
		BVS();
		break;
	case 0x18:
		CLC();
		break;
	case 0xD8:
		CLD();
		break;
	case 0x58:
		CLI();
		break;
	case 0xB8:
		CLV();
		break;
	case 0x38:
		SEC();
		break;
	case 0xF8:
		SED();
		break;
	case 0x78:
		SEI();
		break;
	case 0x40:
		RTI();
		break;
	case 0xEA:
	case 0x04:
	case 0x44:
	case 0x64:
	case 0x0C:
	case 0x14:
	case 0x34:
	case 0x54:
	case 0x74:
	case 0xD4:
	case 0xF4:
	case 0x1A:
	case 0x3A:
	case 0x5A:
	case 0x7A:
	case 0xDA:
	case 0xFA:
	case 0x80:
	case 0x1C:
	case 0x3C:
	case 0x5C:
	case 0x7C:
	case 0xDC:
	case 0xFC:
		NOP(op);
		break;
	case 0x00:
		BRK();
		break;
	case 0xA3:
	case 0xA7:
	case 0xAF:
	case 0xB3:
	case 0xB7:
	case 0xBF:
		LAX(op);
		break;
	case 0x83:
	case 0x87:
	case 0x8F:
	case 0x97:
		SAX(op);
		break;
	case 0xC3:
	case 0xC7:
	case 0xCF:
	case 0xD3:
	case 0xD7:
	case 0xDB:
	case 0xDF:
		DCP(op);
		break;
	case 0xE3:
	case 0xE7:
	case 0xEF:
	case 0xF3:
	case 0xF7:
	case 0xFB:
	case 0xFF:
		ISB(op);
		break;
	case 0x03:
	case 0x07:
	case 0x0F:
	case 0x13:
	case 0x17:
	case 0x1B:
	case 0x1F:
		SLO(op);
		break;
	case 0x23:
	case 0x27:
	case 0x2F:
	case 0x33:
	case 0x37:
	case 0x3B:
	case 0x3F:
		RLA(op);
		break;
	case 0x43:
	case 0x47:
	case 0x4F:
	case 0x53:
	case 0x57:
	case 0x5B:
	case 0x5F:
		SRE(op);
		break;
	case 0x63:
	case 0x67:
	case 0x6F:
	case 0x73:
	case 0x77:
	case 0x7B:
	case 0x7F:
		RRA(op);
		break;
	default:
		break;
	}
}

// LOAD / STORE
void CPU::LDA(uint8_t op)
{
	//std::cout << "LDA ";
	uint8_t value = 0;
	bool pageCrossed = false;
	switch (op)
	{
		case 0xA9: //Immediate
			value = getImmediate();
			cycles += 2;
			break;
		case 0xA5:
			value = getMemory(getZeroPageAddress());
			cycles += 3;
			break;
		case 0xB5:
			value = getMemory((getZeroPageAddress() + X) & 0xFF);
			cycles += 4;
			break;
		case 0xAD:
			value = getMemory(getAbsoluteAddress());
			cycles += 4;
			break;
		case 0xBD:
			value = getMemory(getAbsoluteIndexedAddress(X, pageCrossed));
			cycles += pageCrossed ? 5 : 4;
			break;
		case 0xB9:
			value = getMemory(getAbsoluteIndexedAddress(Y, pageCrossed));
			cycles += pageCrossed ? 5 : 4;
			break;
		case 0xA1:
			value = getMemory(getIndirectAddress());
			cycles += 6;
			break;
		case 0xB1:
			value = getMemory(getIndirectIndexedAddress(pageCrossed));
			cycles += pageCrossed ? 6 : 5;
			break;
	}

	/*std::cout << std::hex << "LDA: " << static_cast<int>(value) << std::endl;*/
	A = value;
	updateZeroNegativeFlags(A);
}
void CPU::LDX(uint8_t op)
{
	//std::cout << "LDX ";
	uint8_t value = 0;
	bool pageCrossed = false;
	switch (op)
	{
		case 0xA2:
			value = getImmediate();
			cycles += 2;
			break;
		case 0xA6:
			value = getMemory(getZeroPageAddress());
			cycles += 3;
			break;
		case 0xB6:
			value = getMemory((getZeroPageAddress() + Y) & 0xFF);
			cycles += 4;
			break;
		case 0xAE:
			value = getMemory(getAbsoluteAddress());
			cycles += 4;
			break;
		case 0xBE:
			value = getMemory(getAbsoluteIndexedAddress(Y, pageCrossed));
			cycles += pageCrossed ? 5 : 4;
			break;
	}

	X = value;
	updateZeroNegativeFlags(X);
}
void CPU::LDY(uint8_t op)
{
	/*std::cout << "LDY ";*/
	uint8_t value = 0;
	bool pageCrossed = false;
	switch (op)
	{
		case 0xA0:
			value = getImmediate();
			cycles += 2;
			break;
		case 0xA4:
			value = getMemory(getZeroPageAddress());
			cycles += 3;
			break;
		case 0xB4:
			value = getMemory((getZeroPageAddress() + X) & 0xFF);
			cycles += 4;
			break;
		case 0xAC:
			value = getMemory(getAbsoluteAddress());
			cycles += 4;
			break;
		case 0xBC:
			value = getMemory(getAbsoluteIndexedAddress(X, pageCrossed));
			cycles += pageCrossed ? 5 : 4;
			break;
	}

	Y = value;
	updateZeroNegativeFlags(Y);
}
void CPU::LAX(uint8_t op)
{
	uint8_t value = 0;
	bool pageCrossed = false;
	switch (op)
	{
		case 0xA7:
			value = getMemory(getZeroPageAddress());
			cycles += 3;
			break;
		case 0xA3:
			value = getMemory(getIndirectAddress());
			cycles += 6;
			break;
		case 0xAF:
			value = getMemory(getAbsoluteAddress());
			cycles += 4;
			break;
		case 0xB3:
			value = getMemory(getIndirectIndexedAddress(pageCrossed));
			cycles += pageCrossed ? 6 : 5;
			break;
		case 0xB7:
			value = getMemory((getZeroPageAddress() + Y) & 0xFF);
			cycles += 4;
			break;
		case 0xBF:
			value = getMemory(getAbsoluteIndexedAddress(Y, pageCrossed));
			cycles += pageCrossed ? 5 : 4;
			break;
	}
	A = value;
	X = value;
	updateZeroNegativeFlags(A);
}
void CPU::STA(uint8_t op)
{
	/*std::cout << "STA ";*/
	uint16_t addr = 0;
	bool pageCrossed = false;
	switch (op)
	{
		case 0x85:
			addr = getZeroPageAddress();
			cycles += 3;
			break;
		case 0x95:
			addr = (getZeroPageAddress() + X) & 0xFF;
			cycles += 4;
			break;
		case 0x8D:
			addr = getAbsoluteAddress();
			cycles += 4;
			break;
		case 0x9D:
			addr = getAbsoluteAddress() + X;
			cycles += 5;
			break;
		case 0x99:
			addr = getAbsoluteAddress() + Y;
			cycles += 5;
			break;
		case 0x81:
			addr = getIndirectAddress();
			cycles += 6;
			break;
		case 0x91:
			addr = getIndirectIndexedAddress(pageCrossed);
			cycles += 6;
			break;
	}
	//std::cout << "STA: " << std::hex << static_cast<int>(A) << " to address: " << addr << std::endl;
	setMemory(addr, A);
}
void CPU::STX(uint8_t op)
{
	/*std::cout << "STX ";*/
	uint16_t addr = 0;
	switch (op)
	{
	case 0x86:
		addr = getZeroPageAddress();
		cycles += 3;
		break;
	case 0x96:
		addr = (getZeroPageAddress() + Y) & 0xFF;
		cycles += 4;
		break;
	case 0x8E:
		addr = getAbsoluteAddress();
		cycles += 4;
		break;
	}
	setMemory(addr, X);
}
void CPU::STY(uint8_t op)
{
	/*std::cout << "STY ";*/
	uint16_t addr = 0;
	switch (op)
	{
	case 0x84:
		addr = getZeroPageAddress();
		cycles += 3;
		break;
	case 0x94:
		addr = (getZeroPageAddress() + X) & 0xFF;
		cycles += 4;
		break;
	case 0x8C:
		addr = getAbsoluteAddress();
		cycles += 4;
		break;
	}
	setMemory(addr, Y);
}
void CPU::SAX(uint8_t op)
{
	uint8_t value = 0;
	uint16_t addr = 0;
	bool pageCrossed = false;
	switch (op)
	{
		case 0x87:
			addr = getZeroPageAddress();
			cycles += 3;
			break;
		case 0x83:
			addr = getIndirectAddress();
			cycles += 6;
			break;
		case 0x8F:
			addr = getAbsoluteAddress();
			cycles += 4;
			break;
		case 0x97:
			addr = (getZeroPageAddress() + Y) & 0xFF;
			cycles += 4;
			break;
	}
	uint8_t result = A & X;
	setMemory(addr, result);
}

// REGISTER TRANSFER
void CPU::TAX()
{
	/*std::cout << "TAX ";*/
	X = A;
	cycles += 2;
	updateZeroNegativeFlags(X);
}
void CPU::TAY()
{
	/*std::cout << "TAY ";*/
	Y = A;
	cycles += 2;
	updateZeroNegativeFlags(Y);
}
void CPU::TXA()
{
	/*std::cout << "TXA ";*/
	A = X;
	cycles += 2;
	updateZeroNegativeFlags(A);
}
void CPU::TYA()
{
	/*std::cout << "TYA ";*/
	A = Y;
	cycles += 2;
	updateZeroNegativeFlags(A);
}

// STACK OPERATIONS
void CPU::TSX()
{
	/*std::cout << "TSX ";*/
	X = SP;
	cycles += 2;
	updateZeroNegativeFlags(X);
}
void CPU::TXS()
{
	/*std::cout << "TXS ";*/
	SP = X;
	cycles += 2;
}
void CPU::PHA()
{
	/*std::cout << "PHA ";*/
	push(A);
	cycles += 3;
}
void CPU::PHP()
{
	/*std::cout << "PHP ";*/
	push(SR | 0x10);
	cycles += 3;
}
void CPU::PLA()
{
	/*std::cout << "PLA ";*/
	A = pull();
	cycles += 4;
	updateZeroNegativeFlags(A);
}
void CPU::PLP() 
{
	/*std::cout << "PLP ";*/
	uint8_t value = pull();
	SR = (value & 0xCF) | U_FLAG;
	cycles += 4;
}

// LOGICAL OPERATIONS
void CPU::AND(uint8_t op)
{
	uint8_t value = 0;
	bool pageCrossed = false;
	switch (op)
	{
		case 0x29:
			value = getImmediate();
			cycles += 2;
			break;
		case 0x25:
			value = getMemory(getZeroPageAddress());
			cycles += 3;
			break;
		case 0x35:
			value = getMemory((getZeroPageAddress() + X) & 0xFF);
			cycles += 4;
			break;
		case 0x2D:
			value = getMemory(getAbsoluteAddress());
			cycles += 4;
			break;
		case 0x3D:
			value = getMemory(getAbsoluteIndexedAddress(X, pageCrossed));
			cycles += pageCrossed ? 5 : 4;
			break;
		case 0X39:
			value = getMemory(getAbsoluteIndexedAddress(Y, pageCrossed));
			cycles += pageCrossed ? 5 : 4;
			break;
		case 0x21:
			value = getMemory(getIndirectAddress());
			cycles += 6;
			break;
		case 0x31:
			value = getMemory(getIndirectIndexedAddress(pageCrossed));
			cycles += pageCrossed ? 6 : 5;
			break;
	}
	A = A & value;
	updateZeroNegativeFlags(A);
}
void CPU::EOR(uint8_t op)
{
	/*std::cout << "EOR ";*/
	uint8_t value = 0;
	bool pageCrossed = false;
	switch (op)
	{
		case 0x49:
			value = getImmediate();
			cycles += 2;
			break;
		case 0x45:
			value = getMemory(getZeroPageAddress());
			cycles += 3;
			break;
		case 0x55:
			value = getMemory((getZeroPageAddress() + X) & 0xFF);
			cycles += 4;
			break;
		case 0x4D:
			value = getMemory(getAbsoluteAddress());
			cycles += 4;
			break;
		case 0x5D:
			value = getMemory(getAbsoluteIndexedAddress(X, pageCrossed));
			cycles += pageCrossed ? 5 : 4;
			break;
		case 0x59:
			value = getMemory(getAbsoluteIndexedAddress(Y, pageCrossed));
			cycles += pageCrossed ? 5 : 4;
			break;
		case 0x41:
			value = getMemory(getIndirectAddress());
			cycles += 6;
			break;
		case 0x51:
			value = getMemory(getIndirectIndexedAddress(pageCrossed));
			cycles += pageCrossed ? 6 : 5;
			break;
	}

	A = A ^ value;
	updateZeroNegativeFlags(A);
}
void CPU::ORA(uint8_t op)
{
	/*std::cout << "ORA ";*/
	uint8_t value = 0;
	bool pageCrossed = false;
	switch (op)
	{
	case 0x09:
		value = getImmediate();
		cycles += 2;
		break;
	case 0x05:
		value = getMemory(getZeroPageAddress());
		cycles += 3;
		break;
	case 0x15:
		value = getMemory((getZeroPageAddress() + X) & 0xFF);
		cycles += 4;
		break;
	case 0x0D:
		value = getMemory(getAbsoluteAddress());
		cycles += 4;
		break;
	case 0x1D:
		value = getMemory(getAbsoluteIndexedAddress(X, pageCrossed));
		cycles += pageCrossed ? 5 : 4;
		break;
	case 0x19:
		value = getMemory(getAbsoluteIndexedAddress(Y, pageCrossed));
		cycles += pageCrossed ? 5 : 4;
		break;
	case 0x01:
		value = getMemory(getIndirectAddress());
		cycles += 6;
		break;
	case 0x11:
		value = getMemory(getIndirectIndexedAddress(pageCrossed));
		cycles += pageCrossed ? 6 : 5;
		break;
	}
	//std::cout << "A = " << std::hex << static_cast<int>(A) << ", value = " << std::hex << static_cast<int>(value) << std::endl;
	A = A | value;
	updateZeroNegativeFlags(A);
}
void CPU::BIT(uint8_t op)
{
	/*std::cout << "BIT ";*/
	uint8_t value = 0;
	switch (op)
	{
		case 0x24:
			value = getMemory(getZeroPageAddress());
			cycles += 3;
			break;
		case 0x2C:
			value = getMemory(getAbsoluteAddress());
			cycles += 4;
			break;
	}
	updateZeroNegativeFlags(A & value);
	SR = (SR & ~V_FLAG) | (value & V_FLAG);
	SR = (SR & ~N_FLAG) | (value & N_FLAG);
}

// ARITHMETIC
void CPU::ADC(uint8_t op)
{
	/*std::cout << "ADC ";*/
	uint8_t value = 0;
	bool pageCrossed = false;
	switch (op)
	{
	case 0x69:
		value = getImmediate();
		cycles += 2;
		break;
	case 0x65:
		value = getMemory(getZeroPageAddress());
		cycles += 3;
		break;
	case 0x75:
		value = getMemory((getZeroPageAddress() + X) & 0xFF);
		cycles += 4;
		break;
	case 0x6D:
		value = getMemory(getAbsoluteAddress());
		cycles += 4;
		break;
	case 0x7D:
		value = getMemory(getAbsoluteIndexedAddress(X, pageCrossed));
		cycles += pageCrossed ? 5 : 4;
		break;
	case 0x79:
		value = getMemory(getAbsoluteIndexedAddress(Y, pageCrossed));
		cycles += pageCrossed ? 5 : 4;
		break;
	case 0x61:
		value = getMemory(getIndirectAddress());
		cycles += 6;
		break;
	case 0x71:
		value = getMemory(getIndirectIndexedAddress(pageCrossed));
		cycles += pageCrossed ? 6 : 5;
		break;
	}

	//std::cout << "A = " << std::hex << static_cast<int>(A) << ", value = " << std::hex << static_cast<int>(value) << std::endl;

	uint8_t oldA = A;
	uint16_t result = A + value + (SR & 0x01);
	A = result & 0xFF;
	updateADCFlags(oldA, value, result);
}
void CPU::SBC(uint8_t op)
{
	/*std::cout << "SBC ";*/
	uint8_t value = 0;
	bool pageCrossed = false;
	switch (op)
	{
		case 0xE9:
		case 0xEB:
			value = getImmediate();
			cycles += 2;
			break;
		case 0xE5:
			value = getMemory(getZeroPageAddress());
			cycles += 3;
			break;
		case 0xF5:
			value = getMemory((getZeroPageAddress() + X) & 0xFF);
			cycles += 4;
			break;
		case 0xED:
			value = getMemory(getAbsoluteAddress());
			cycles += 4;
			break;
		case 0xFD:
			value = getMemory(getAbsoluteIndexedAddress(X, pageCrossed));
			cycles += pageCrossed ? 5 : 4;
			break;
		case 0xF9:
			value = getMemory(getAbsoluteIndexedAddress(Y, pageCrossed));
			cycles += pageCrossed ? 5 : 4;
			break;
		case 0xE1:
			value = getMemory(getIndirectAddress());
			cycles += 6;
			break;
		case 0xF1:
			value = getMemory(getIndirectIndexedAddress(pageCrossed));
			cycles += pageCrossed ? 6 : 5;
			break;
	}
	uint8_t oldA = A;
	uint16_t result = A + ~value + (SR & C_FLAG);
	A = result & 0xFF;
	updateSBCFlags(oldA, value, result);
}
void CPU::CMP(uint8_t op)
{
	/*std::cout << "CMP ";*/
	uint8_t value = 0;
	bool pageCrossed = false;
	switch (op)
	{
		case 0xC9:
			value = getImmediate();
			cycles += 2;
			break;
		case 0xC5:
			value = getMemory(getZeroPageAddress());
			cycles += 3;
			break;
		case 0xD5:
			value = getMemory((getZeroPageAddress() + X) & 0xFF);
			cycles += 4;
			break;
		case 0xCD:
			value = getMemory(getAbsoluteAddress());
			cycles += 4;
			break;
		case 0xDD:
			value = getMemory(getAbsoluteIndexedAddress(X, pageCrossed));
			cycles += pageCrossed ? 5 : 4;
			break;
		case 0xD9:
			value = getMemory(getAbsoluteIndexedAddress(Y, pageCrossed));
			cycles += pageCrossed ? 5 : 4;
			break;
		case 0xC1:
			value = getMemory(getIndirectAddress());
			cycles += 6;
			break;
		case 0xD1:
			value = getMemory(getIndirectIndexedAddress(pageCrossed));
			cycles += pageCrossed ? 6 : 5;
			break;
	}
	updateCMPFlags(value, A);
}
void CPU::CPX(uint8_t op)
{
	/*std::cout << "CPX ";*/
	uint8_t value = 0;
	bool pageCrossed = false;
	switch (op)
	{
		case 0xE0:
			value = getImmediate();
			cycles += 2;
			break;
		case 0xE4:
			value = getMemory(getZeroPageAddress());
			cycles += 3;
			break;
		case 0xEC:
			value = getMemory(getAbsoluteAddress());
			cycles += 4;
			break;
	}
	updateCMPFlags(value, X);
}
void CPU::CPY(uint8_t op)
{
	/*std::cout << "CPY ";*/
	uint8_t value = 0;
	bool pageCrossed = false;
	switch (op)
	{
	case 0xC0:
		value = getImmediate();
		cycles += 2;
		break;
	case 0xC4:
		value = getMemory(getZeroPageAddress());
		cycles += 3;
		break;
	case 0xCC:
		value = getMemory(getAbsoluteAddress());
		cycles += 4;
		break;
	}
	updateCMPFlags(value, Y);
}
void CPU::ISB(uint8_t op)
{
	uint8_t value = 0;
	uint16_t addr = 0;
	bool pageCrossed = false;
	switch (op)
	{
		case 0xE3:
			addr = getIndirectAddress();
			cycles += 8;
			break;
		case 0xE7:
			addr = getZeroPageAddress();
			cycles += 5;
			break;
		case 0xEF:
			addr = getAbsoluteAddress();
			cycles += 6;
			break;
		case 0xF3:
			addr = getIndirectIndexedAddress(pageCrossed);
			cycles += pageCrossed ? 8 : 7;
			break;
		case 0xF7:
			addr = (getZeroPageAddress() + X) & 0xFF;
			cycles += 6;
			break;
		case 0xFB:
			addr = getAbsoluteIndexedAddress(Y, pageCrossed);
			cycles += pageCrossed ? 7 : 6;
			break;
		case 0xFF:
			addr = getAbsoluteAddress() + X;
			cycles += 7;
			break;
	}

	value = getMemory(addr);
	value += 1;
	setMemory(addr, value);

	uint16_t result = A + ~value + (SR & C_FLAG);
	updateSBCFlags(A, value, result);
	A = result & 0xFF;
}

// INCREMENTS & DECREMENTS
void CPU::INC(uint8_t op)
{
	/*std::cout << "INC ";*/
	uint16_t address = 0;
	switch (op)
	{
		case 0xE6:
			address = getZeroPageAddress();
			cycles += 5;
			break;
		case 0xF6:
			address = (getZeroPageAddress() + X) & 0xFF;
			cycles += 6;
			break;
		case 0xEE:
			address = getAbsoluteAddress();
			cycles += 6;
			break;
		case 0xFE:
			address = getAbsoluteAddress() + X;
			cycles += 7;
			break;
	}

	setMemory(address, getMemory(address) + 1);
	updateZeroNegativeFlags(getMemory(address));
}
void CPU::INX()
{
	/*std::cout << "INX ";*/
	X += 1;
	cycles += 2;
	updateZeroNegativeFlags(X);
}
void CPU::INY()
{
	/*std::cout << "INY ";*/
	Y += 1;
	cycles += 2;
	updateZeroNegativeFlags(Y);
}
void CPU::DEC(uint8_t op)
{
	/*std::cout << "DEC ";*/
	uint16_t address = 0;
	switch (op)
	{
	case 0xC6:
		address = getZeroPageAddress();
		cycles += 5;
		break;
	case 0xD6:
		address = (getZeroPageAddress() + X) & 0xFF;
		cycles += 6;
		break;
	case 0xCE:
		address = getAbsoluteAddress();
		cycles += 6;
		break;
	case 0xDE:
		address = getAbsoluteAddress() + X;
		cycles += 7;
		break;
	}

	setMemory(address, getMemory(address) - 1);
	updateZeroNegativeFlags(getMemory(address));
}
void CPU::DEX()
{
	/*std::cout << "DEX ";*/
	X -= 1;
	cycles += 2;
	updateZeroNegativeFlags(X);
}
void CPU::DEY()
{
	/*std::cout << "DEY ";*/
	Y -= 1;
	cycles += 2;
	updateZeroNegativeFlags(Y);
}
void CPU::DCP(uint8_t op)
{
	uint16_t addr = 0;
	bool pageCrossed = false;
	switch (op)
	{
		case 0xC3:
			addr = getIndirectAddress();
			cycles += 8;
			break;
		case 0xC7:
			addr = getZeroPageAddress();
			cycles += 5;
			break;
		case 0xCF:
			addr = getAbsoluteAddress();
			cycles += 6;
			break;
		case 0xD3:
			addr = getIndirectIndexedAddress(pageCrossed);
			cycles += pageCrossed ? 8 : 7;
			break;
		case 0xD7:
			addr = (getZeroPageAddress() + X) & 0xFF;
			cycles += 6;
			break;
		case 0xDB:
			addr = getAbsoluteIndexedAddress(Y, pageCrossed);
			cycles += pageCrossed ? 7 : 6;
			break;
		case 0xDF:
			addr = getAbsoluteAddress() + X;
			cycles += 7;
			break;
	}
	uint8_t value = getMemory(addr);
	value -= 1;
	setMemory(addr, value);
	updateCMPFlags(value, A);
}

// SHIFTS
void CPU::ASL(uint8_t op) {
	/*std::cout << "ASL ";*/
	uint8_t value = 0;
	uint16_t address = 0;
	switch (op) {
	case 0x0A:
		value = A;
		A = value << 1;
		updateShiftFlags(value, A);
		cycles += 2;
		break;
	case 0x06:
		address = getZeroPageAddress();
		value = getMemory(address);
		setMemory(address, value << 1);
		updateShiftFlags(value, getMemory(address));
		cycles += 5;
		break;
	case 0x16:
		address = (getZeroPageAddress() + X) & 0xFF;
		value = getMemory(address);
		setMemory(address, value << 1);
		updateShiftFlags(value, getMemory(address));
		cycles += 6;
		break;
	case 0x0E:
		address = getAbsoluteAddress();
		value = getMemory(address);
		setMemory(address, value << 1);
		updateShiftFlags(value, getMemory(address));
		cycles += 6;
		break;
	case 0x1E:
		address = getAbsoluteAddress() + X;
		value = getMemory(address);
		setMemory(address, value << 1);
		updateShiftFlags(value, getMemory(address));
		cycles += 7;
		break;
	}
}
void CPU::LSR(uint8_t op)
{
	/*std::cout << "LSR ";*/
	uint8_t value = 0;
	uint16_t address = 0;
	switch (op)
	{
	case 0x4A:
		value = A;
		A = value >> 1;
		cycles += 2;
		break;
	case 0x46:
		address = getZeroPageAddress();
		value = getMemory(address);
		setMemory(address, value >> 1);
		cycles += 5;
		break;
	case 0x56:
		address = (getZeroPageAddress() + X) & 0xFF;
		value = getMemory(address);
		setMemory(address, value >> 1);
		cycles += 6;
		break;
	case 0x4E:
		address = getAbsoluteAddress();
		value = getMemory(address);
		setMemory(address, value >> 1);
		cycles += 6;
		break;
	case 0x5E:
		address = getAbsoluteAddress() + X;
		value = getMemory(address);
		setMemory(address, value >> 1);
		cycles += 7;
		break;
	}
	updateLSRFlags(value, value >> 1);
}
void CPU::ROR(uint8_t op)
{
	/*std::cout << "ROR ";*/
	uint8_t value = 0;
	uint16_t address = 0;
	switch (op)
	{
		case 0x6A: 
			value = A;
			A = ((SR & C_FLAG) << 7) | (value >> 1);
			cycles += 2;
			break;
		case 0x66: 
			address = getZeroPageAddress();
			value = getMemory(address);
			setMemory(address, ((SR & C_FLAG) << 7) | (value >> 1));
			cycles += 5;
			break;
		case 0x76: 
			address = (getZeroPageAddress() + X) & 0xFF;
			value = getMemory(address);
			setMemory(address, ((SR & C_FLAG) << 7) | (value >> 1));
			cycles += 6;
			break;
		case 0x6E: 
			address = getAbsoluteAddress();
			value = getMemory(address);
			setMemory(address, ((SR & C_FLAG) << 7) | (value >> 1));
			cycles += 6;
			break;
		case 0x7E:
			address = getAbsoluteAddress() + X;
			value = getMemory(address);
			setMemory(address, ((SR & C_FLAG) << 7) | (value >> 1));
			cycles += 7;
			break;
	}
	updateLSRFlags(value, ((SR & C_FLAG) << 7) | (value >> 1));
}
void CPU::ROL(uint8_t op)
{
	/*std::cout << "ROL ";*/
	uint8_t value = 0;
	uint16_t address = 0;
	switch (op)
	{
		case 0x2A: 
			value = A;
			A = (value << 1) | (SR & C_FLAG);
			cycles += 2;
			updateShiftFlags(value, A);
			break;
		case 0x26: 
			address = getZeroPageAddress();
			value = getMemory(address);
			setMemory(address, (value << 1) | (SR & C_FLAG));
			cycles += 5;
			updateShiftFlags(value, getMemory(address));
			break;
		case 0x36: 
			address = (getZeroPageAddress() + X) & 0xFF;
			value = getMemory(address);
			setMemory(address, (value << 1) | (SR & C_FLAG));
			cycles += 6;
			updateShiftFlags(value, getMemory(address));
			break;
		case 0x2E: 
			address = getAbsoluteAddress();
			value = getMemory(address);
			setMemory(address, (value << 1) | (SR & C_FLAG));
			cycles += 6;
			updateShiftFlags(value, getMemory(address));
			break;
		case 0x3E:
			address = getAbsoluteAddress() + X;
			value = getMemory(address);
			setMemory(address, (value << 1) | (SR & C_FLAG));
			cycles += 7;
			updateShiftFlags(value, getMemory(address));
			break;
	}
}
void CPU::SLO(uint8_t op)
{
	uint8_t value = 0;
	uint16_t addr = 0;
	bool pageCrossed = false;
	switch (op)
	{
		case 0x03:
			addr = getIndirectAddress();
			cycles += 8;
			break;
		case 0x07:
			addr = getZeroPageAddress();
			cycles += 5;
			break;
		case 0x0F:
			addr = getAbsoluteAddress();
			cycles += 6;
			break;
		case 0x13:
			addr = getIndirectIndexedAddress(pageCrossed);
			cycles += pageCrossed ? 8 : 7;
			break;
		case 0x17:
			addr = (getZeroPageAddress() + X) & 0xFF;
			cycles += 6;
			break;
		case 0x1B:
			addr = getAbsoluteIndexedAddress(Y, pageCrossed);
			cycles += pageCrossed ? 7 : 6;
			break;
		case 0x1F:
			addr = getAbsoluteAddress() + X;
			cycles += 7;
			break;
	}

	value = getMemory(addr);
	updateShiftFlags(value, value << 1);
	value <<= 1;
	setMemory(addr, value);

	A |= value;
	updateZeroNegativeFlags(A);
}
void CPU::RLA(uint8_t op)
{
	uint8_t value = 0;
	uint16_t addr = 0;
	bool pageCrossed = false;
	switch (op)
	{
		case 0x23:
			addr = getIndirectAddress();
			cycles += 8;
			break;
		case 0x27:
			addr = getZeroPageAddress();
			cycles += 5;
			break;
		case 0x2F:
			addr = getAbsoluteAddress();
			cycles += 6;
			break;
		case 0x33:
			addr = getIndirectIndexedAddress(pageCrossed);
			cycles += pageCrossed ? 8 : 7;
			break;
		case 0x37:
			addr = (getZeroPageAddress() + X) & 0xFF;
			cycles += 6;
			break;
		case 0x3B:
			addr = getAbsoluteIndexedAddress(Y, pageCrossed);
			cycles += pageCrossed ? 7 : 6;
			break;
		case 0x3F:
			addr = getAbsoluteAddress() + X;
			cycles += 7;
			break;
	}

	value = getMemory(addr);
	setMemory(addr, (value << 1) | (SR & C_FLAG));
	updateShiftFlags(value, getMemory(addr));
	
	A &= getMemory(addr);
	updateZeroNegativeFlags(A);
}
void CPU::SRE(uint8_t op)
{
	uint8_t value = 0;
	uint16_t addr = 0;
	bool pageCrossed = false;
	switch (op)
	{
		case 0x43:
			addr = getIndirectAddress();
			cycles += 8;
			break;
		case 0x47:
			addr = getZeroPageAddress();
			cycles += 5;
			break;
		case 0x4F:
			addr = getAbsoluteAddress();
			cycles += 6;
			break;
		case 0x53:
			addr = getIndirectIndexedAddress(pageCrossed);
			cycles += 8;
			break;
		case 0x57:
			addr = (getZeroPageAddress() + X) & 0xFF;
			cycles += 6;
			break;
		case 0x5B:
			addr = getAbsoluteIndexedAddress(Y, pageCrossed);
			cycles += 7;
			break;
		case 0x5F:
			addr = getAbsoluteAddress() + X;
			cycles += 7;
			break;
	}

	value = getMemory(addr);
	SR &= ~C_FLAG;
	SR |= (value & 0x01);
	value >>= 1;
	setMemory(addr, value);

	A ^= value;
	updateZeroNegativeFlags(A);
}
void CPU::RRA(uint8_t op)
{
	uint8_t value = 0;
	uint16_t addr = 0;
	bool pageCrossed = false;
	switch (op)
	{
		case 0x63:
			addr = getIndirectAddress();
			cycles += 8;
			break;
		case 0x67:
			addr = getZeroPageAddress();
			cycles += 5;
			break;
		case 0x6F:
			addr = getAbsoluteAddress();
			cycles += 6;
			break;
		case 0x73:
			addr = getIndirectIndexedAddress(pageCrossed);
			cycles += 8;
			break;
		case 0x77:
			addr = (getZeroPageAddress() + X) & 0xFF;
			cycles += 6;
			break;
		case 0x7B:
			addr = getAbsoluteIndexedAddress(Y, pageCrossed);
			cycles += 7;
			break;
		case 0x7F:
			addr = getAbsoluteAddress() + X;
			cycles += 7;
			break;
	}

	value = getMemory(addr);
	setMemory(addr, ((SR & C_FLAG) << 7) | (value >> 1));
	updateLSRFlags(value, getMemory(addr));

	uint16_t result = A + getMemory(addr) + (SR & C_FLAG);
	updateADCFlags(A, getMemory(addr), result);
	A = result & 0xFF;
}

// JUMPS & CALLS
void CPU::JMP(uint8_t op)
{
	/*std::cout << "JMP ";*/
	switch (op)
	{
		case 0x4C:
			PC = getAbsoluteAddress();
			cycles += 3;
			break;
		default:
			uint16_t pointer = getAbsoluteAddress();
			uint8_t low = getMemory(pointer);
			uint8_t high = getMemory((pointer & 0xFF00) | ((pointer + 1) & 0xFF));
			PC = (static_cast<uint16_t>(high) << 8) | low;
			cycles += 5;
			break;
	}
}
void CPU::JSR()
{
	/*std::cout << "JSR ";*/
	uint16_t target = getAbsoluteAddress();
	uint16_t ret = PC - 1;
	push((ret >> 8) & 0xFF);
	push(ret & 0xFF);
	PC = target;
	cycles += 6;
}
void CPU::RTS()
{
	/*std::cout << "RTS ";*/
	uint8_t low = pull();
	uint8_t high = pull();
	PC = (static_cast<uint16_t>(high) << 8) | low;
	PC++;
	cycles += 6;
}

// BRANCHES
void CPU::BCC()
{
	/*std::cout << "BCC ";*/
	int8_t offset = static_cast<int8_t>(getImmediate()); 
	cycles += 2;
	if (!(SR & C_FLAG)) 
	{
		uint16_t oldPC = PC;
		PC += offset;
		cycles += 1; 
		if ((oldPC & 0xFF00) != (PC & 0xFF00)) 
		{
			cycles += 1;
		}
	}
}
void CPU::BCS()
{
	/*std::cout << "BCS ";*/
	int8_t offset = static_cast<int8_t>(getImmediate());
	cycles += 2;
	if (SR & C_FLAG)
	{
		uint16_t oldPC = PC;
		PC += offset;
		cycles += 1;
		if ((oldPC & 0xFF00) != (PC & 0xFF00))
		{
			cycles += 2;
		}
	}
}
void CPU::BEQ()
{
	/*std::cout << "BEQ ";*/
	int8_t offset = static_cast<int8_t>(getImmediate());
	cycles += 2;
	if (SR & Z_FLAG)
	{
		uint16_t oldPC = PC;
		PC += offset;
		cycles += 1;
		if ((oldPC & 0xFF00) != (PC & 0xFF00))
		{
			cycles += 1;
		}
	}
}
void CPU::BMI()
{
	/*std::cout << "BMI ";*/
	int8_t offset = static_cast<int8_t>(getImmediate());
	cycles += 2;
	if (SR & N_FLAG)
	{
		uint16_t oldPC = PC;
		PC += offset;
		cycles += 1;
		if ((oldPC & 0xFF00) != (PC & 0xFF00))
		{
			cycles += 2;
		}
	}
}
void CPU::BNE()
{
	/*std::cout << "BNE ";*/
	int8_t offset = static_cast<int8_t>(getImmediate());
	cycles += 2;
	if (!(SR & Z_FLAG))
	{
		uint16_t oldPC = PC;
		PC += offset;
		cycles += 1;
		if ((oldPC & 0xFF00) != (PC & 0xFF00))
		{
			cycles += 2;
		}
	}
}
void CPU::BPL()
{
	/*std::cout << "BPL ";*/
	int8_t offset = static_cast<int8_t>(getImmediate());
	cycles += 2;
	if (!(SR & N_FLAG))
	{
		uint16_t oldPC = PC;
		PC += offset;
		cycles += 1;
		if ((oldPC & 0xFF00) != (PC & 0xFF00))
		{
			cycles += 2;
		}
	}
}
void CPU::BVC()
{
	/*std::cout << "BVC ";*/
	int8_t offset = static_cast<int8_t>(getImmediate());
	cycles += 2;
	if (!(SR & V_FLAG))
	{
		uint16_t oldPC = PC;
		PC += offset;
		cycles += 1;
		if ((oldPC & 0xFF00) != (PC & 0xFF00))
		{
			cycles += 2;
		}
	}
}
void CPU::BVS()
{
	/*std::cout << "BVS ";*/
	int8_t offset = static_cast<int8_t>(getImmediate());
	cycles += 2;
	if (SR & V_FLAG)
	{
		uint16_t oldPC = PC;
		PC += offset;
		cycles += 1;
		if ((oldPC & 0xFF00) != (PC & 0xFF00))
		{
			cycles += 2;
		}
	}
}

// STATUS FLAG CHANGES
void CPU::CLC()
{
	/*std::cout << "CLC ";*/
	SR &= ~C_FLAG;
	cycles += 2;
}
void CPU::CLD()
{
	/*std::cout << "CLD ";*/
	SR &= ~D_FLAG;
	cycles += 2;
}
void CPU::CLI()
{
	/*std::cout << "CLI ";*/
	SR &= ~I_FLAG;
	cycles += 2;
}
void CPU::CLV()
{
	/*std::cout << "CLV ";*/
	SR &= ~V_FLAG;
	cycles += 2;
}
void CPU::SEC()
{
	/*std::cout << "SEC ";*/
	SR |= C_FLAG;
	cycles += 2;
}
void CPU::SED()
{
	/*std::cout << "SED ";*/
	SR |= D_FLAG;
	cycles += 2;
}
void CPU::SEI()
{
	/*std::cout << "SEI ";*/
	SR |= I_FLAG;
	cycles += 2;
}

// SYSTEM
void CPU::BRK()
{
	/*std::cout << "BRK ";*/
	PC += 2;
	SR |= B_FLAG;
	push((PC >> 8) & 0xFF);
	push(PC & 0xFF);
	push(SR);
	PC = getMemory(0xFFFE) | (static_cast<uint16_t>(getMemory(0xFFFF)) << 8);
	SR |= I_FLAG;
	cycles += 7;
}
void CPU::NOP(uint8_t op)
{
	/*std::cout << "NOP ";*/
	bool pageCrossed = false;
	switch (op)
	{
		case 0xEA: // Standard NOP
		case 0x1A:
		case 0x3A:
		case 0x5A:
		case 0x7A:
		case 0xDA:
		case 0xFA:
			cycles += 2;
			break;
		case 0x80:
			cycles += 2;
			PC++;
			break;
		case 0x04:
		case 0x44:
		case 0x64:
			cycles += 3; // Zero Page NOPs
			PC++;
			break;
		case 0x0C:
			cycles += 4;
			PC += 2; // Absolute NOP
			break;
		case 0x14:
		case 0x34:
		case 0x54:
		case 0x74:
		case 0xD4:
		case 0xF4:
			cycles += 4; // Zero Page Indexed NOPs
			PC++;
			break;
		case 0x1C:
		case 0x3C:
		case 0x5C:
		case 0x7C:
		case 0xDC:
		case 0xFC:
			getAbsoluteIndexedAddress(X, pageCrossed);
			cycles += pageCrossed ? 5 : 4; // Absolute Indexed NOPs  MIGHT NEED TO ACCOUNT FOR PAGE CROSSING
			break;
	}
}
void CPU::RTI()
{
	/*std::cout << "RTI ";*/
	SR = pull();
	SR &= ~B_FLAG; // Clear Break flag
	SR |= U_FLAG; // Set Unused flag
	uint8_t low = pull();
	uint8_t high = pull();
	PC = low | (static_cast<uint16_t>(high) << 8);
	cycles += 6;
}

uint8_t CPU::getImmediate() 
{
	return getMemory(PC++);
}

uint16_t CPU::getZeroPageAddress() 
{
	return getMemory(PC++);
}

uint16_t CPU::getAbsoluteAddress() 
{
	uint8_t low = getMemory(PC++);
	uint8_t high = getMemory(PC++);
	return (static_cast<uint16_t>(high) << 8) | low;
}

uint16_t CPU::getIndirectAddress()
{
	uint8_t zAddr = (getZeroPageAddress() + X) & 0xFF;
	uint8_t low = getMemory(zAddr & 0xFF);
	uint8_t high = getMemory((zAddr + 1) & 0xFF);
	return (static_cast<uint16_t>(high) << 8) | low;
}

uint16_t CPU::getAbsoluteIndexedAddress(uint8_t index, bool& pageCrossed)
{
	uint8_t low = getMemory(PC++);
	uint8_t high = getMemory(PC++);
	uint16_t baseAddress = (static_cast<uint16_t>(high) << 8) | low;
	uint16_t effectiveAddress = baseAddress + index;
	pageCrossed = (baseAddress & 0xFF00) != (effectiveAddress & 0xFF00); // Compare high bytes
	return effectiveAddress;
}

uint16_t CPU::getIndirectIndexedAddress(bool& pageCrossed)
{
	uint8_t zAddr = getZeroPageAddress();
	uint8_t lo = getMemory(zAddr);
	uint8_t hi = getMemory((zAddr + 1) & 0xFF); // wrap around zero-page
	uint16_t baseAddress = (static_cast<uint16_t>(hi) << 8) | lo;
	uint16_t effectiveAddress = baseAddress + Y;

	pageCrossed = (baseAddress & 0xFF00) != (effectiveAddress & 0xFF00);
	return effectiveAddress;
}

void CPU::updateZeroNegativeFlags(uint8_t value)
{
	SR = (SR & ~(Z_FLAG | N_FLAG)); // Clear Zero and Negative flags
	if (value == 0) SR |= Z_FLAG; // Set Zero flag
	if (value & N_FLAG) SR |= N_FLAG; // Set Negative flag
}

void CPU::updateCMPFlags(uint8_t value, uint8_t A)
{
	//std::cout << "CMP A = " << std::hex << static_cast<int>(A) << ", value = " << std::hex << static_cast<int>(value) << std::endl;
	uint16_t result = A - value; // Unsigned subtraction
	SR = (SR & ~(C_FLAG | Z_FLAG | N_FLAG)); // Clear C, Z, N
	if (A >= value) SR |= C_FLAG; // Set Carry if A >= M
	if (value == A) SR |= Z_FLAG; // Set Zero if A == M
	if (result & 0x80) SR |= N_FLAG; // Set Negative if bit 7 of result is 1
}

void CPU::updateADCFlags(uint8_t oldA, uint8_t value, uint16_t result) {
	SR = (SR & ~(C_FLAG | Z_FLAG | V_FLAG | N_FLAG)); // Clear C, Z, V, N
	if (result > 0xFF) SR |= C_FLAG; // Set Carry flag
	if ((result & 0xFF) == 0) SR |= Z_FLAG; // Fix: Set Zero flag
	if ((oldA ^ result) & (value ^ result) & 0x80) SR |= V_FLAG; // Set Overflow flag
	if (result & 0x80) SR |= N_FLAG; // Set Negative flag
}

void CPU::updateSBCFlags(uint8_t oldA, uint8_t value, uint16_t result) {
	SR = (SR & ~(C_FLAG | Z_FLAG | V_FLAG | N_FLAG)); // Clear C, Z, V, N
	if (result <= 0xFF) SR |= C_FLAG; // Set Carry if no borrow
	if (result == 0) SR |= Z_FLAG; // Set Zero flag
	if (((result ^ oldA) & (result ^ ~value)) & 0x80) SR |= V_FLAG; // Set Overflow
	if (result & N_FLAG) SR |= N_FLAG; // Set Negative flag
}

void CPU::updateASLFlags(uint8_t value) {
	SR = (SR & ~(C_FLAG | Z_FLAG | N_FLAG)); // Clear C, Z, N
	if (value & 0x80) SR |= C_FLAG; // Set Carry if bit 7 was 1
	if (value == 0) SR |= Z_FLAG; // Set Zero flag
	if (value & 0x80) SR |= N_FLAG; // Set Negative flag
}

void CPU::updateLSRFlags(uint8_t oldValue, uint8_t newValue) {
	SR = (SR & ~(C_FLAG | Z_FLAG | N_FLAG)); // Clear C, Z, N
	if (oldValue & C_FLAG) SR |= C_FLAG; // Set Carry if bit 0 was 1
	if (newValue == 0) SR |= Z_FLAG; // Set Zero flag
	if (newValue & N_FLAG) SR |= N_FLAG; // Set Negative flag
}

void CPU::updateShiftFlags(uint8_t oldValue, uint8_t newValue) {
	SR = (SR & ~(C_FLAG | Z_FLAG | N_FLAG)); // Clear C, Z, N
	if (oldValue & 0x80) SR |= C_FLAG; // Set Carry if bit 7 was 1 (for ASL/ROL)
	if (newValue == 0) SR |= Z_FLAG; // Set Zero flag based on result
	if (newValue & 0x80) SR |= N_FLAG; // Set Negative flag based on result
}

void CPU::push(uint8_t value)
{
	setMemory(0x0100 + SP, value);
	SP = (SP - 1) & 0xFF;
}

uint8_t CPU::pull()
{
	SP = (SP + 1) & 0xFF;
	return getMemory(0x0100 + SP);
}

void CPU::setMemory(uint16_t address, uint8_t value) {
	memory->write(address, value);
}

uint8_t CPU::getMemory(uint16_t address) {
	return memory->read(address);
}

std::string CPU::getOpName(uint8_t op) const
{
	switch (op)
	{
	case 0xA9: // LDA Immediate
	case 0xA5: // LDA Zero Page
	case 0xB5:
	case 0xAD: // LDA Absolute
	case 0xBD:
	case 0xB9:
	case 0xA1:
	case 0xB1:
		return "LDA";
		break;
	case 0xA2:
	case 0xA6:
	case 0xB6:
	case 0xAE:
	case 0xBE:
		return "LDX";
		break;
	case 0xA0:
	case 0xA4:
	case 0xB4:
	case 0xAC:
	case 0xBC:
		return "LDY";
		break;
	case 0x85:
	case 0x95:
	case 0x8D:
	case 0x9D:
	case 0x99:
	case 0x81:
	case 0x91:
		return "STA";
		break;
	case 0x86:
	case 0x96:
	case 0x8E:
		return "STX";
		break;
	case 0x84:
	case 0x94:
	case 0x8C:
		return "STY";
		break;
	case 0xAA:
		return "TAX";
		break;
	case 0xA8:
		return "TAY";
		break;
	case 0x8A:
		return "TXA";
		break;
	case 0x98:
		return "TYA";
		break;
	case 0xBA:
		return "TSX";
		break;
	case 0x9A:
		return "TXS";
		break;
	case 0x48:
		return "PHA";
		break;
	case 0x08:
		return "PHP";
		break;
	case 0x68:
		return "PLA";
		break;
	case 0x28:
		return "PLP";
		break;
	case 0x29:
	case 0x25:
	case 0x35:
	case 0x2D:
	case 0x3D:
	case 0X39:
	case 0x21:
	case 0x31:
		return "AND";
		break;
	case 0x49:
	case 0x45:
	case 0x55:
	case 0x4D:
	case 0x5D:
	case 0x59:
	case 0x41:
	case 0x51:
		return "EOR";
		break;
	case 0x09:
	case 0x05:
	case 0x15:
	case 0x0D:
	case 0x1D:
	case 0x19:
	case 0x01:
	case 0x11:
		return "ORA";
		break;
	case 0x24:
	case 0x2C:
		return "BIT";
		break;
	case 0x69:
	case 0x65:
	case 0x75:
	case 0x6D:
	case 0x7D:
	case 0x79:
	case 0x61:
	case 0x71:
		return "ADC";
		break;
	case 0xE9:
	case 0xE5:
	case 0xF5:
	case 0xED:
	case 0xFD:
	case 0xF9:
	case 0xE1:
	case 0xF1:
		return "SBC";
		break;
	case 0xC9:
	case 0xC5:
	case 0xD5:
	case 0xCD:
	case 0xDD:
	case 0xD9:
	case 0xC1:
	case 0xD1:
		return "CMP";
		break;
	case 0xE0:
	case 0xE4:
	case 0xEC:
		return "CPX";
		break;
	case 0xC0:
	case 0xC4:
	case 0xCC:
		return "CPY";
		break;
	case 0xE6:
	case 0xF6:
	case 0xEE:
	case 0xFE:
		return "INC";
		break;
	case 0xE8:
		return "INX";
		break;
	case 0xC8:
		return "INY";
		break;
	case 0xC6:
	case 0xD6:
	case 0xCE:
	case 0xDE:
		return "DEC";
		break;
	case 0xCA:
		return "DEX";
		break;
	case 0x88:
		return "DEY";
		break;
	case 0x0A:
	case 0x06:
	case 0x16:
	case 0x0E:
	case 0x1E:
		return "ASL";
		break;
	case 0x4A:
	case 0x46:
	case 0x56:
	case 0x4E:
	case 0x5E:
		return "LSR";
		break;
	case 0x2A:
	case 0x26:
	case 0x36:
	case 0x2E:
	case 0x3E:
		return "ROL";
		break;
	case 0x6A:
	case 0x66:
	case 0x76:
	case 0x6E:
	case 0x7E:
		return "ROR";
		break;
	case 0x4C:
	case 0x6C:
		return "JMP";
		break;
	case 0x20:
		return "JSR";
		break;
	case 0x60:
		return "RTS";
		break;
	case 0x90:
		return "BCC";
		break;
	case 0xB0:
		return "BCS";
		break;
	case 0xF0:
		return "BEQ";
		break;
	case 0x30:
		return "BMI";
		break;
	case 0xD0:
		return "BNE";
		break;
	case 0x10:
		return "BPL";
		break;
	case 0x50:
		return "BVC";
		break;
	case 0x70:
		return "BVS";
		break;
	case 0x18:
		return "CLC";
		break;
	case 0xD8:
		return "CLD";
		break;
	case 0x58:
		return "CLI";
		break;
	case 0xB8:
		return "CLV";
		break;
	case 0x38:
		return "SEC";
		break;
	case 0xF8:
		return "SED";
		break;
	case 0x78:
		return "SEI";
		break;
	case 0x40:
		return "RTI";
		break;
	case 0xEA:
		return "NOP";
		break;
	case 0x04:
	case 0x44:
	case 0x64:
	case 0x0C:
	case 0x14:
	case 0x34:
	case 0x54:
	case 0x74:
	case 0xD4:
	case 0xF4:
	case 0x1A:
	case 0x3A:
	case 0x5A:
	case 0x7A:
	case 0xDA:
	case 0xFA:
	case 0x80:
	case 0x1C:
	case 0x3C:
	case 0x5C:
	case 0x7C:
	case 0xDC:
	case 0xFC:
		return "*NOP";
		break;
	case 0x00:
		return "BRK";
		break;
	case 0xA3:
	case 0xA7:
	case 0xAF:
	case 0xB3:
	case 0xB7:
	case 0xBF:
		return "LAX";
		break;
	case 0x83:
	case 0x87:
	case 0x8F:
	case 0x97:
		return "SAX";
		break;
	case 0xEB:
		return "*SBC";
		break;
	case 0xC3:
	case 0xC7:
	case 0xCF:
	case 0xD3:
	case 0xD7:
	case 0xDB:
	case 0xDF:
		return "DCP";
		break;
	case 0xE3:
	case 0xE7:
	case 0xEF:
	case 0xF3:
	case 0xF7:
	case 0xFB:
	case 0xFF:
		return "ISB";
		break;
	case 0x03:
	case 0x07:
	case 0x0F:
	case 0x13:
	case 0x17:
	case 0x1B:
	case 0x1F:
		return "SLO";
		break;
	case 0x23:
	case 0x27:
	case 0x2F:
	case 0x33:
	case 0x37:
	case 0x3B:
	case 0x3F:
		return "RLA";
		break;
	case 0x43:
	case 0x47:
	case 0x4F:
	case 0x53:
	case 0x57:
	case 0x5B:
	case 0x5F:
		return "SRE";
		break;
	case 0x63:
	case 0x67:
	case 0x6F:
	case 0x73:
	case 0x77:
	case 0x7B:
	case 0x7F:
		return "RRA";
		break;
	default:
		return "XXX";
		break;
	}
}

void CPU::logState(std::string opName) {

	/*Format opcode bytes like "A2 00 00"*/
	std::ostringstream opHex;
	for (int i = 0; i < 3; ++i)
	{
		//if (i < length)
		//	opHex << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(opcodeBytes[i]) << " ";
		//else
			opHex << "   ";
	}

	// Format line
	std::ostringstream line;
	line << std::uppercase << std::hex << std::setfill(' ')
		<< std::setw(4) << PC << "  "
		<< opHex.str() << opName
		<< std::left << std::setw(32) << " " << std::setfill('0')
		<< "A:" << std::setw(2) << int(A) << " "
		<< "X:" << std::setw(2) << int(X) << " "
		<< "Y:" << std::setw(2) << int(Y) << " "
		<< "P:" << std::setw(2) << int(SR | 0x20) << " "
		<< "SP:" << std::setw(2) << int(SP) << " " << std::setfill(' ')
		<< "PPU:" << std::right << std::setw(3) << std::dec << ppu->getScanline() << "," << std::setw(3) << ppu->getCycle() << " "
		<< "CYC:" << std::dec << cycles;

	std::cout << line.str() << std::endl;
}