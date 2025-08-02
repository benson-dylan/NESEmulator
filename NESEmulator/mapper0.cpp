#include "mapper.h"

class Mapper0 : public Mapper
{
public:
	Mapper0(uint8_t prgBanks, uint8_t chrBanks)
		: Mapper(prgBanks, chrBanks) {}

	bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override
	{
		if (addr >= 0x8000 && addr <= 0xFFFF)
		{
			mappedAddr = addr - 0x8000;
			if (prgBanks == 1)
				mappedAddr %= 0x4000;
			return true;
		}
		return false;
	}

	bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr) override
	{
		return false;
	}

	bool chrMapRead(uint16_t addr, uint32_t& mappedAddr) override
	{
		if (addr < 0x2000)
		{
			mappedAddr = addr;
			return true;
		}
		return false;
	}

	bool chrMapWrite(uint16_t addr, uint32_t& mappedAddr) override
	{
		if (addr < 0x2000 && chrBanks == 0)
		{
			mappedAddr = addr;
			return true;
		}
		return false;
	}

	int mapperID() const override { return 0; }

};