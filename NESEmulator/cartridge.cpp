#include "cartridge.h"
#include "mapper0.cpp"
#include <iostream>
#include <fstream>


bool Cartridge::loadROM(std::string filename)
{
	std::ifstream rom(filename, std::ios::binary);
	
	if (!rom)
	{
		std::cerr << "Failed to open file\n";
		return false;
	}

	char header[16];
	rom.read(header, 16);

	if (header[0] != 'N' || header[1] != 'E' || header[2] != 'S' || header[3] != 0x1A)
	{
		std::cerr << "Invalid NES File\n";
		return false;
	}

	int prgBanks = header[4];
	int chrBanks = header[5];
	prgSize = prgBanks * 16384;
	chrSize = chrBanks * 8192;
	uint8_t flag6 = header[6];
	uint8_t flag7 = header[7];

	hasTrainer = (flag6 & 0x04) != 0;
	hasBattery = (flag6 & 0x02) != 0;

	if (flag6 & 0x08)
		mirroring = FourScreen;
	else if (flag6 & 0x01)
		mirroring = Vertical;
	else
		mirroring = Horizontal;

	uint8_t mapperLow = (flag6 >> 4);
	uint8_t mapperHigh = (flag7 >> 4);
	uint8_t mapperID = mapperHigh << 4 | mapperLow;

	if (hasTrainer)
	{
		std::cout << "TRAINER FOUND\n";
		trainer.resize(512);
		rom.read(reinterpret_cast<char*>(trainer.data()), trainer.size());
	}

	prgROM.resize(prgSize);
	rom.read(reinterpret_cast<char*>(prgROM.data()), prgROM.size());

	//for (uint8_t byte : prgROM)
	//	std::cout << std::hex << (int)byte << std::endl;

	if (chrSize > 0)
	{
		chrROM.resize(chrSize);
		rom.read(reinterpret_cast<char*>(chrROM.data()), chrSize);
		chrRAM.clear();  // Ensure RAM is empty when using ROM
		usesCHR_RAM = false;
	}
	else
	{

		chrROM.clear();  // Ensure ROM is empty when using RAM
		chrRAM.resize(8192);  // 8KB of CHR RAM
		std::fill(chrRAM.begin(), chrRAM.end(), 0);  // Initialize RAM to 0
		usesCHR_RAM = true;
	}

	switch (mapperID)
	{
		case 0:
			mapper = std::make_unique<Mapper0>(prgBanks, chrBanks);
			break;
		default:
			return false;
	}

	std::cout << "ROM Loaded Successfully.\n";
	return true;
}

uint8_t Cartridge::cpuRead(uint16_t addr)
{
	uint32_t mappedAddr = 0;
	if (mapper->cpuMapRead(addr, mappedAddr))
		return prgROM[mappedAddr];
	return 0;
}

void Cartridge::cpuWrite(uint16_t addr, uint8_t data)
{
	uint32_t mappedAddr = 0;
	if (mapper->cpuMapWrite(addr, mappedAddr))
		prgROM[mappedAddr] = data;
}

uint8_t Cartridge::chrRead(uint16_t addr)
{
	uint32_t mappedAddr = 0;
	if (mapper->chrMapRead(addr, mappedAddr))
	{
		if (usesCHR_RAM)
		{
			return chrRAM[mappedAddr];  // Read from RAM
		}
		else
		{
			return chrROM[mappedAddr];  // Read from ROM
		}
	}
	return 0x00;
}

void Cartridge::chrWrite(uint16_t addr, uint8_t data)
{
	uint32_t mappedAddr = 0;
	if (mapper->chrMapWrite(addr, mappedAddr))
	{
		if (usesCHR_RAM)
		{
			chrRAM[mappedAddr] = data;  // Write to RAM
		}
		else if (!usesCHR_RAM)
		{
			// Optionally: Add warning or handle attempt to write to ROM
			//chrROM[mappedAddr] = data;  // Writing to ROM is usually not allowed
		}
	}
}

Cartridge::MirroringType Cartridge::getMode()
{
	return mirroring;
}

void Cartridge::setMode(MirroringType mode)
{
	mirroring = mode;
}

int Cartridge::getMapperID()
{
	return mapperID;
}