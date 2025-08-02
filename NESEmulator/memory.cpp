#include "memory.h"
#include <iostream>

Memory::Memory(Cartridge* cart, PPU* ppu, APU* apu)
{
	this->cartridge = cart;
	this->ppu = ppu;
	this->apu = apu;
}

// For debug only!
Memory::Memory(Cartridge* cart, PPU* ppu)
{
	this->cartridge = cart;
	this->ppu = ppu;
}

// For debug only!
Memory::Memory(Cartridge* cart)
{
	this->cartridge = cart;
}

uint8_t Memory::read(uint16_t addr)
{
	if (addr <= 0x1FFF)
	{
		return ram[addr % 0x0800];
	}
	else if (addr >= 0x2000 && addr <= 0x3FFF)
	{
		uint16_t reg = addr % 8;
		//std::cout << "Reading from address: " << std::hex << reg << std::endl;
		return ppu->readRegister(reg);
	}
	else if (addr >= 0x4000 && addr <= 0x401F)
	{
		//return apu->readRegister(addr);
	}
	else if (addr >= 0x4020 && addr <= 0xFFFF)
	{
		return cartridge->cpuRead(addr);
	}

	return 0;
}

void Memory::write(uint16_t addr, uint8_t data)
{
	//std::cout << "Write to address: " << std::hex << addr << " with data: " << std::hex << (int)data << std::endl;
	if (addr <= 0x1FFF)
	{
		ram[addr % 0x0800] = data;
	}
	else if (addr >= 0x2000 && addr <= 0x3FFF)
	{
		uint16_t reg = addr % 8;
		//std::cout << "Write to PPU register: " << reg << " with data: " << (int)data << std::endl;
		ppu->writeRegister(reg, data);
	}
	else if (addr >= 0x4000 && addr <= 0x401F)
	{
		//apu->writeRegister(addr, data);
	}
	else if (addr >= 0x4020 && addr <= 0xFFFF)
	{
		cartridge->cpuWrite(addr, data);
	}
}

