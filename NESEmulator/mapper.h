#pragma once
#include <cstdint>

class Mapper
{
protected:
	uint8_t prgBanks;
	uint8_t chrBanks;

public:
	Mapper(uint8_t prgBanks, uint8_t chrBanks) 
		: prgBanks(prgBanks), chrBanks(chrBanks) {}
	
	virtual ~Mapper() = default;

	virtual bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) = 0;
	virtual bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr) = 0;

	virtual bool chrMapRead(uint16_t addr, uint32_t& mappedAddr) = 0;
	virtual bool chrMapWrite(uint16_t addr, uint32_t& mappedAddr) = 0;

	virtual int mapperID() const = 0;
};

