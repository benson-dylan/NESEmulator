#include "apu.h"
#include <iostream>
APU::APU()
{
	std::fill(std::begin(registers), std::end(registers), 0);
}

void APU::writeRegister(uint16_t addr, uint8_t value)
{
	//std::cout << "APU Write to address: " << std::hex << addr << " with data: " << std::hex << (int)value << std::endl;
	registers[addr - 0x4000] = value;
}

uint8_t APU::readRegister(uint16_t addr)
{
	//std::cout << "APU Read from address: " << std::hex << addr << std::endl;
	return registers[addr - 0x4000];
}
