#include "ppu.h"

PPU::PPU(Cartridge* cart)
{
	ppuctrl = 0x00; // Control Register
	ppumask = 0x00; // Mask Register
	ppustatus = 0xA0; // Status Register
	oamaddr = 0x00; // OAM Address Register

	latch = 0x00; // Latch for reading data
	vramAddr = 0x0000; // VRAM Address
	tempAddr = 0x0000; // Temporary VRAM Address
	fineX = 0x00; // Fine X Scroll
	dataBuffer = 0x00; // Data Buffer

	std::fill(std::begin(oamData), std::end(oamData), 0);
	std::fill(std::begin(paletteRAM), std::end(paletteRAM), 0);
	std::fill(std::begin(nameTables), std::end(nameTables), 0);
	std::fill(std::begin(frameBuffer), std::end(frameBuffer), 0);
	std::fill(std::begin(spriteScanline), std::end(spriteScanline), Sprite{ 0, 0, 0, 0, 0, 0 });

	spriteCount = 0;

	scanline = 0; // Current scanline
	cycle = 0; // Current cycle

	nmiOccurred = false; // NMI flag
	nmiOutput = false; // NMI output flag
	nmiPrevious = false; // Previous NMI output state
	nmiDelay = false; // NMI delay flag
	frameComplete = false; // Frame complete flag

	dmaPage = 0x00; // DMA Page
	dmaTriggered = false;

	cartridge = cart;

	bgPatternShiftHigh = 0x00; // Background pattern shift high
	bgPatternShiftLow = 0x00; // Background pattern shift low
	bgAttribShiftHigh = 0x00; // Background attribute shift high
	bgAttribShiftLow = 0x00; // Background attribute shift low
}

void PPU::step(uint32_t cpuCycles)
{
	//std::cout << "[PPU] Step called with " << std::dec << cpuCycles << " CPU cycles." << std::endl;
	for (uint32_t i = 0; i < cpuCycles * 3; ++i)
	{
		/* --- Rendered Per Cycle --- */

		if (scanline < 240 && cycle >= 1 && cycle <= 256 && (ppumask & 0x18))
		{
			if ((ppumask & 0x08) || (ppumask & 0x10))
			{
				bgPatternShiftLow <<= 1;
				bgPatternShiftHigh <<= 1;
				bgAttribShiftLow <<= 1;
				bgAttribShiftHigh <<= 1;
			}

			switch (cycle % 8)
			{
				case 0:
				{
					bgPatternShiftLow = (bgPatternShiftLow << 8) | nextTileLSB;
					bgPatternShiftHigh = (bgPatternShiftHigh << 8) | nextTileMSB;

					uint8_t attrLowBit = (nextTileAttrib & 1) ? 0xFF : 0x00;
					uint8_t attrHighBit = (nextTileAttrib & 2) ? 0xFF : 0x00;
					bgAttribShiftLow = (bgAttribShiftLow << 8) | attrLowBit;
					bgAttribShiftHigh = (bgAttribShiftHigh << 8) | attrHighBit;

					incrementScrollX();
					break;
				}
				case 1:
					nextTileID = readVRAM(0x2000 | (vramAddr & 0x0FFF));
					/*std::cout << "NT Read Addr: " << std::hex << (0x2000 | (vramAddr & 0x0FFF))
						<< ", Tile ID: " << int(nextTileID) << "\n";*/
					break;
				case 3: {
					uint16_t attrAddr = 0x23C0 | (vramAddr & 0x0C00) | ((vramAddr >> 4) & 0x38) | ((vramAddr >> 2) & 0x07);
					uint8_t attrByte = readVRAM(attrAddr);

					int shift = ((vramAddr >> 4) & 4) | (vramAddr & 2);
					nextTileAttrib = (attrByte >> shift) & 0x03;
					break;
				}
				case 5:
				{
					uint16_t fineY = (vramAddr >> 12) & 0x07;
					uint16_t baseTable = (ppuctrl & 0x10) ? 0x1000 : 0x0000;
					uint16_t tileAddr = baseTable + nextTileID * 16 + fineY;
					nextTileLSB = readVRAM(tileAddr);
					break;
				}
				case 7:
				{
					uint16_t fineY = (vramAddr >> 12) & 0x07;
					uint16_t baseTable = (ppuctrl & 0x10) ? 0x1000 : 0x0000;
					uint16_t tileAddr = baseTable + nextTileID * 16 + fineY;
					nextTileMSB = readVRAM(tileAddr + 8); // <-- Fetch MSB here

					break;
				}
			}
			renderPixel();
		}

		if (scanline < 240 || scanline == 261)
		{
			if (cycle == 256)
				incrementScrollY();

			if (cycle == 257)
				copyHorizontalScrollBits();
		}

		if (scanline < 240 && cycle == 257)
		{
			evaluateSprites();
			fetchSpritePatterns();
		}

		/* --- Scanline Transition --- */
		cycle++;

		if (cycle >= 341)
		{
			// Scanline complete, reset cycle
			cycle = 0;
			scanline++;

			// Debug frame buffer dump
			//if (scanline == 239) {
			//	std::ofstream dump("frame.txt");
			//	for (int y = 0; y < 240; y++) {
			//		for (int x = 0; x < 256; x++) {
			//			dump << std::hex << frameBuffer[y * 256 + x] << " ";
			//		}
			//		dump << "\n";
			//	}
			//}

			// Handle Vblank and NMI
			if (scanline == 241)
			{
				//std::cout << "VBLANK TIME!!\n";
				ppustatus |= 0x80;
				if (nmiOutput)
				{
					nmiOccurred = true;
					nmiDelay = 2;
				}
			}

			// Clear VBlank
			if (scanline == 261)
			{
				ppustatus &= ~0x80; // VBlank Clear
				ppustatus &= ~0x40; // Sprite Overflow Clear
				ppustatus &= ~0x20; // Sprite 0 Hit Clear
				nmiOccurred = false;
				nmiDelay = 0;
			}

			// Vertical Scroll Bits Copy
			if (scanline == 261 && cycle >= 280 && cycle <= 304)
				copyVerticalScrollBits();

			// Frame complete
			if (scanline >= 262)
			{
				scanline = 0;
				frameComplete = true;

				/*std::cout << "CTRL: " << std::hex << (int)ppuctrl
					<< " Pattern Table Addr: " << ((ppuctrl & 0x10) ? "0x1000" : "0x0000") << std::endl;*/
			}

		}

		/*if (scanline == 100 && cycle == 1) {
			printf("vramAddr: %04X, fineX: %d, ppuctrl: %02X, ppumask: %02X\n", vramAddr, fineX, ppuctrl, ppumask);
		}*/
	}
}

uint8_t PPU::readRegister(uint16_t addr)
{
	uint8_t result = 0;
	addr = addr + 0x2000;
	switch (addr)
	{
		case 0x2002: // PPUSTATUS
			result = ppustatus;
			ppustatus &= ~0x80;
			latch = 0;
			//std::cout << "[CPU] Read from $2002 (PPUSTATUS): " << std::hex << int(result) << std::endl;
			return result;
		case 0x2004: // OAMDATA
			return oamData[oamaddr];
		case 0x2007: // PPUDATA
			result = dataBuffer;
			dataBuffer = readVRAM(vramAddr);

			if (vramAddr >= 0x3F00) 
				result = readVRAM(vramAddr); 

			vramAddr += (ppuctrl & 0x04) ? 32 : 1;
			return result;
		default:
			return 0x00;
	}
}

void PPU::writeRegister(uint16_t addr, uint8_t value)
{
	addr = addr + 0x2000;
	switch (addr)
	{
		case 0x2000: // PPUCTRL
			ppuctrl = value;
			tempAddr = (tempAddr & 0xF3FF) | ((value & 0x30) << 10);
			nmiOutput = (value & 0x80) != 0;
			//std::cout << "[CPU] Wrote to $2000 (PPUCTRL): " << std::hex << int(value) << std::endl;
			break;
		case 0x2001: // PPUMASK
			ppumask = value;
			//std::cout << "[CPU] Wrote to $2001 (PPUMASK): " << std::hex << int(value) << std::endl;
			break;
		case 0x2003: // OAMADDR
			oamaddr = value;
			break;
		case 0x2004: // OAMDATA
			oamData[oamaddr++] = value;
			//std::cout << "[CPU] Wrote to $2004 (OAMDATA): " << std::hex << int(value) << " at OAM address " << std::dec << int(oamaddr) << std::endl;
			break;
		case 0x2005: // PPUSCROLL
			if (latch == 0)
			{
				fineX = value & 0x07;
				tempAddr = (tempAddr & 0xFFE0) | (value >> 3);
				latch = 1;
			}
			else
			{
				tempAddr = (tempAddr & 0x8FFF) | ((value & 0x07) << 12);
				tempAddr = (tempAddr & 0xFC1F) | ((value & 0xF8) << 2);
				latch = 0;
			}
			break;
		case 0x2006: // PPUADDR
			if (latch == 0) {
				tempAddr = (tempAddr & 0x00FF) | ((value & 0x3F) << 8);
				latch = 1;
			}
			else {
				tempAddr = (tempAddr & 0xFF00) | value;
				vramAddr = tempAddr;
				latch = 0;
			}
			break;
		case 0x2007: // PPUDATA
			writeVRAM(vramAddr, value);
			vramAddr += (ppuctrl & 0x04) ? 32 : 1; // increment mode
			break;
		case 0x4014: // OAMDMA
			dmaPage = value;
			dmaTriggered = true;
			break;
	}
}

uint8_t PPU::readVRAM(uint16_t addr)
{
	addr &= 0x3FFF;

	if (addr < 0x2000)
	{
		// Pattern Table Reads
		return cartridge->chrRead(addr);
	}
	else if (addr < 0x3000)
	{

		uint16_t mirroredAddr = mirrorNametableAddress(addr);
		return nameTables[mirroredAddr]; // Name Tables
	}
	else if (addr < 0x3F00)
	{
		uint16_t mirroredAddr = mirrorNametableAddress(addr - 0x1000);
		return nameTables[mirroredAddr];
	}
	else if (addr < 0x4000)
	{
		return paletteRAM[mirrorPaletteAddress(addr)]; // Palette RAM
	}

	return 0;
}

void PPU::writeVRAM(uint16_t addr, uint8_t value)
{
	addr &= 0x3FFF;
	if (addr < 0x2000)
	{
		cartridge->chrWrite(addr, value);
	}
	else if (addr < 0x3F00)
	{
		uint16_t mirroredAddr = mirrorNametableAddress(addr);
		nameTables[mirroredAddr] = value; // Name Tables
	}
	else if (addr < 0x4000)
	{
		paletteRAM[mirrorPaletteAddress(addr)] = value; // Palette RAM
	}
}

uint16_t PPU::mirrorNametableAddress(uint16_t addr)
{
	addr = addr & 0x0FFF;
	switch (cartridge->getMode())
	{
		case Cartridge::MirroringType::Vertical:
			return (addr & 0x07FF);
		case Cartridge::MirroringType::Horizontal:
			return (addr & 0x03FF) | ((addr & 0x0800) >> 1);
		case Cartridge::MirroringType::SingleScreenUpper:
			return (addr & 0x03FF) | 0x0400;
		case Cartridge::MirroringType::SingleScreenLower:
			return (addr & 0x03FF);
		case Cartridge::MirroringType::FourScreen:
			return addr;
		default:
			return addr & 0x07FF; // Fallback to no mirroring
	}
}

uint8_t PPU::mirrorPaletteAddress(uint16_t addr)
{
	addr &= 0x1F;
	if (addr == 0x10) return 0x00;
	if (addr == 0x14) return 0x04;
	if (addr == 0x18) return 0x08;
	if (addr == 0x1C) return 0x0C;
	return addr;
}

void PPU::incrementScrollX()
{
	if ((vramAddr & 0x001F) == 31)
	{
		vramAddr &= ~0x001F; 
		vramAddr ^= 0x0400; 
	}
	else
	{
		vramAddr += 1; 
	}
}

void PPU::incrementScrollY()
{
	if ((vramAddr & 0x7000) != 0x7000) 
	{
		vramAddr += 0x1000; 
	}
	else 
	{
		vramAddr &= ~0x7000; 
		uint16_t y = (vramAddr & 0x03E0) >> 5; 
		if (y == 29) 
		{
			y = 0;
			vramAddr ^= 0x0800; 
		}
		else if (y == 31) 
		{
			y = 0; 
		}
		else
		{
			y++;
		}
		vramAddr = (vramAddr & ~0x03E0) | (y << 5);
	}
}

bool PPU::getNMI()
{
	if (nmiDelay > 0)
	{
		nmiDelay--;
		if (nmiDelay == 0 && nmiOccurred && nmiOutput)
		{
			printf("PPU triggered NMI at scanline %d, cycle %d\n", scanline, cycle);
			return true; // NMI triggered
		}
	}
	return false; // No NMI triggered
}

void PPU::copyHorizontalScrollBits()
{
	vramAddr = (vramAddr & 0xFBE0) | (tempAddr & 0x041F);
}

void PPU::copyVerticalScrollBits()
{
	vramAddr = (vramAddr & 0x841F) | (tempAddr & 0x7BE0);
}

void PPU::evaluateSprites()
{
	//std::cout << "[PPU] Evaluating sprites for scanline " << scanline << std::endl;
	spriteCount = 0;
	spriteZeroHit = false;
	uint8_t spriteHeight = (ppuctrl & 0x20) ? 16 : 8;

	for (int i = 0; i < 64; ++i)
	{
		uint8_t y = oamData[i * 4];
		if (scanline >= y && scanline < (uint16_t)(y + spriteHeight))
		{
			if (spriteCount < 8)
			{
				spriteScanline[spriteCount].y = oamData[i * 4];
				spriteScanline[spriteCount].tileID = oamData[i * 4 + 1];
				spriteScanline[spriteCount].attributes = oamData[i * 4 + 2];
				spriteScanline[spriteCount].x = oamData[i * 4 + 3];

				/*std::cout << "Scanline " << scanline
					<< " Sprite " << spriteCount
					<< " (OAM " << i << "): Y=" << (int)spriteScanline[spriteCount].y
					<< " X=" << (int)spriteScanline[spriteCount].x
					<< " Tile=" << (int)spriteScanline[spriteCount].tileID
					<< " Attr=" << std::hex << (int)spriteScanline[spriteCount].attributes
					<< std::dec << std::endl;*/

				if (i == 0)
					spriteZeroHit = true;
				spriteCount++;
			}
			else
			{
				ppustatus |= 0x20;
				break;
			}
		}
	}
}

void PPU::fetchSpritePatterns()
{
	uint8_t spriteHeight = (ppuctrl & 0x20) ? 16 : 8;

	for (int i = 0; i < spriteCount; ++i)
	{
		Sprite& sprite = spriteScanline[i];

		uint8_t tileIndex = sprite.tileID;
		uint8_t attributes = sprite.attributes;
		uint8_t yOffset = scanline - sprite.y;
		bool flipVert = attributes & 0x80;

		/*std::cout << "[PPU] Fetching sprite pattern for sprite " << i
			<< " (OAM " << (i * 4) << "): Y=" << (int)sprite.y
			<< " X=" << (int)sprite.x
			<< " Tile=" << (int)sprite.tileID
			<< " Attr=" << std::hex << (int)sprite.attributes
			<< std::dec << std::endl;*/

		if (flipVert)
		{
			yOffset = spriteHeight - 1 - yOffset;
		}

		uint16_t addr = 0;

		if (spriteHeight == 16)
		{
			uint8_t table = tileIndex & 0x01;
			tileIndex &= 0xFE;

			if (yOffset >= 8)
			{
				tileIndex += 1;
				yOffset -= 8;
			}

			addr = (table * 0x1000) + tileIndex * 16 + yOffset;
		}
		else
		{
			uint16_t patternBase = (ppuctrl & 0x08) ? 0x1000 : 0x0000;
			addr = patternBase + tileIndex * 16 + yOffset;
		}

		sprite.patternLow = readVRAM(addr);
		sprite.patternHigh = readVRAM(addr + 8);
	}
}

void PPU::renderPixel()
{
    int x = cycle - 1;
    int y = scanline;

    // Background Rendering
    uint8_t bit0 = (bgPatternShiftLow >> (15 - fineX)) & 1;
    uint8_t bit1 = (bgPatternShiftHigh >> (15 - fineX)) & 1;
    uint8_t paletteIndex = (bit1 << 1) | bit0;

    uint8_t atrr0 = (bgAttribShiftLow >> (15 - fineX)) & 1;
    uint8_t atrr1 = (bgAttribShiftHigh >> (15 - fineX)) & 1;
    uint8_t paletteHighBits = (atrr1 << 1) | atrr0;

    uint8_t paletteAddr = (paletteHighBits << 2) | paletteIndex;

    if (paletteIndex == 0)
        paletteAddr = 0x00;

    bool bgOpaque = (paletteIndex != 0);
    uint8_t bgColor = readVRAM(0x3F00 + paletteAddr);
    if (!bgOpaque) bgColor = readVRAM(0x3F00);

    // Sprite Rendering
    bool spriteVisible = false;
    uint8_t spriteColor = 0x00;
    bool spritePriority = false;

    if (ppumask & 0x10)  // Check if sprites are enabled
    {
        for (int i = 0; i < spriteCount; ++i)
        {
            int spriteX = spriteScanline[i].x;
            int offset = x - spriteX;
            if (offset < 0 || offset >= 8) continue;

            bool flipH = spriteScanline[i].attributes & 0x40;
            int bitIndex = flipH ? offset : (7 - offset);

            uint8_t spriteBit0 = (spriteScanline[i].patternLow >> bitIndex) & 1;
            uint8_t spriteBit1 = (spriteScanline[i].patternHigh >> bitIndex) & 1;
            uint8_t spritePixel = (spriteBit1 << 1) | spriteBit0;

            if (spritePixel == 0) continue;

            uint8_t spritePalette = spriteScanline[i].attributes & 0x03;
            spriteColor = readVRAM(0x3F10 + (spritePalette << 2) + spritePixel);
            spritePriority = !(spriteScanline[i].attributes & 0x20);
            spriteVisible = true;

            // Sprite 0 hit detection
            if (i == 0 && bgOpaque && x < 255 && (ppumask & 0x18) == 0x18)
            {
                ppustatus |= 0x40;
            }

            break;  // Use first non-transparent sprite pixel
        }
    }

    // Final pixel selection based on priority and opacity
    uint8_t finalColor;
    bool bgEnabled = (ppumask & 0x08) && (x >= 8 || (ppumask & 0x02));
    bool spriteEnabled = (ppumask & 0x10) && (x >= 8 || (ppumask & 0x04));

    if (!bgEnabled && !spriteEnabled)
    {
        finalColor = readVRAM(0x3F00);  // Background color
    }
    else if (!bgEnabled)
    {
        finalColor = spriteVisible ? spriteColor : readVRAM(0x3F00);
    }
    else if (!spriteEnabled)
    {
        finalColor = bgColor;
    }
    else
    {
        if (!bgOpaque && spriteVisible)
        {
            finalColor = spriteColor;
        }
        else if (!spriteVisible)
        {
            finalColor = bgColor;
        }
        else if (spritePriority)
        {
            finalColor = spriteColor;
        }
        else
        {
            finalColor = bgColor;
        }
    }

    frameBuffer[y * 256 + x] = NESPalette[finalColor];
}

// For debug
void PPU::dumpPatternTable(std::array<uint32_t, 128 * 128>& outBuffer, int tableIndex)
{
	uint16_t baseAddr = tableIndex * 0x1000;

	for (int tileY = 0; tileY < 16; ++tileY) {
		for (int tileX = 0; tileX < 16; ++tileX) {
			int tileIndex = tileY * 16 + tileX;
			uint16_t tileAddr = baseAddr + tileIndex * 16;

			for (int row = 0; row < 8; ++row) {
				uint8_t plane0 = cartridge->chrRead(tileAddr + row);
				uint8_t plane1 = cartridge->chrRead(tileAddr + row + 8);

				for (int col = 0; col < 8; ++col) {
					uint8_t bit0 = (plane0 >> (7 - col)) & 1;
					uint8_t bit1 = (plane1 >> (7 - col)) & 1;
					uint8_t colorIndex = (bit1 << 1) | bit0;

					int px = tileX * 8 + col;
					int py = tileY * 8 + row;

					outBuffer[py * 128 + px] = NESPalette[colorIndex];
				}
			}
		}
	}
}