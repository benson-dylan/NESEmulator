#pragma once
#include <cstdint>
#include <array>
#include <string>
#include "cartridge.h"
#include "memory.h"

class CPU
{
public:
	CPU(Memory* memory, PPU* ppu);
	void reset();
	void step();
	void setMemory(uint16_t address, uint8_t value);
	uint8_t getMemory(uint16_t address);
	uint16_t getPC() const { return PC; }
	uint8_t getA() const { return A; }
	uint8_t getSR() const { return SR; }
	uint8_t getX() const { return X; }
	uint8_t getY() const { return Y; }
	uint64_t getCycles() const { return cycles; }
	void handleNMI();
private:
	uint8_t A, X, Y, SP, SR;
	uint16_t PC;
	uint64_t cycles;
	std::array<uint8_t, 2048> ram;
	Cartridge cartridge;
	Memory* memory;
	PPU* ppu;
	bool nmiPending;
	bool irqPending;

	std::string opName;

	//std::array<uint8_t, 0x10000> memory;

	void handleIRQ();

	void execute(uint8_t op);

	// Load & Store
	void LDA(uint8_t op);
	void LDX(uint8_t op);
	void LDY(uint8_t op);
	void LAX(uint8_t op);
	void STA(uint8_t op);
	void STX(uint8_t op);
	void STY(uint8_t op);
	void SAX(uint8_t op);

	// Register Transfer
	void TAX();
	void TAY();
	void TXA();
	void TYA();

	// Stack Operations
	void TSX();
	void TXS();
	void PHA();
	void PHP();
	void PLA();
	void PLP();

	// Logical Operations
	void AND(uint8_t op);
	void EOR(uint8_t op);
	void ORA(uint8_t op);
	void BIT(uint8_t op);

	// Arithmetic
	void ADC(uint8_t op);
	void SBC(uint8_t op);
	void CMP(uint8_t op);
	void CPX(uint8_t op);
	void CPY(uint8_t op);
	void ISB(uint8_t op);

	// Increments & Decrements
	void INC(uint8_t op);
	void INX();
	void INY();
	void DEC(uint8_t op);
	void DEX();
	void DEY();
	void DCP(uint8_t op);

	// Shifts
	void ASL(uint8_t op);
	void LSR(uint8_t op);
	void ROL(uint8_t op);
	void ROR(uint8_t op);
	void SLO(uint8_t op);
	void RLA(uint8_t op);
	void SRE(uint8_t op);
	void RRA(uint8_t op);

	// Jumps & Calls
	void JMP(uint8_t op);
	void JSR();
	void RTS();

	// Branches
	void BCC();
	void BCS();
	void BEQ();
	void BMI();
	void BNE();
	void BPL();
	void BVC();
	void BVS();

	// Status Flag Changes
	void CLC();
	void CLD();
	void CLI();
	void CLV();
	void SEC();
	void SED();
	void SEI();

	// System Functions
	void BRK();
	void NOP(uint8_t op);
	void RTI();

	// Instruction Helpers
	uint8_t getImmediate();
	uint16_t getZeroPageAddress();
	uint16_t getAbsoluteAddress();
	uint16_t getIndirectAddress();
	uint16_t getAbsoluteIndexedAddress(uint8_t index, bool& pageCrossed);
	uint16_t getIndirectIndexedAddress(bool& pageCrossed);

	// Stack Helpers
	void push(uint8_t value);
	uint8_t pull();

	void updateZeroNegativeFlags(uint8_t value);
	void updateADCFlags(uint8_t oldA, uint8_t value, uint16_t result);
	void updateSBCFlags(uint8_t oldA, uint8_t value, uint16_t result);
	void updateCMPFlags(uint8_t value, uint8_t A);
	void updateASLFlags(uint8_t value);
	void updateLSRFlags(uint8_t oldValue, uint8_t newValue);
	void updateShiftFlags(uint8_t oldValue, uint8_t newValue);

	void logState(std::string opName);
	std::string getOpName(uint8_t op) const;
};
