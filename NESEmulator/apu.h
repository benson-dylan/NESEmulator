#pragma once
#include <cstdint>
#include <array>
class APU
{
private:
	std::array<uint8_t, 24> registers;
public:
	APU();
	void writeRegister(uint16_t addr, uint8_t value);
	uint8_t readRegister(uint16_t addr);
};

