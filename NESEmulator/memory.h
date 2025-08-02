#pragma once
#include <cstdint>
#include "cartridge.h"
#include "ppu.h"
#include "apu.h"

class Memory
{
private:
	uint8_t ram[2048];
	Cartridge* cartridge;
	PPU* ppu;
	APU* apu;

public:
	Memory(Cartridge* cart, PPU* ppu, APU* apu);
	Memory(Cartridge* cart, PPU* ppu); // For debug only!
	Memory(Cartridge* cart); // For debug only!

	uint8_t read(uint16_t addr);
	void write(uint16_t addr, uint8_t data);
};

