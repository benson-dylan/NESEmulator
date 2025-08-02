#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <memory>
#include "mapper.h"

class Cartridge
{
private:
	std::vector<uint8_t> prgROM;
	std::vector<uint8_t> chrROM;
	std::vector<uint8_t> chrRAM;
	std::vector<uint8_t> trainer;

	int prgSize;
	int chrSize;
	int mapperID;
	bool hasTrainer;
	bool hasBattery;
	bool usesCHR_RAM;

	std::unique_ptr<Mapper> mapper;

public:
	enum MirroringType {
		Horizontal, Vertical,
		SingleScreenLower,
		SingleScreenUpper,
		FourScreen
	} mirroring;

	bool loadROM(std::string filepath);

	uint8_t cpuRead(uint16_t addr);
	void cpuWrite(uint16_t addr, uint8_t data);

	uint8_t chrRead(uint16_t addr);
	void chrWrite(uint16_t addr, uint8_t data);

	MirroringType getMode();
	void setMode(MirroringType mode);

	int getMapperID();
};

