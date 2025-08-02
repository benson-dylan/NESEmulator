#include <iostream>
#include <iomanip>
#include <SDL.h>
#include "cpu.h"
#include "cartridge.h"
#include "memory.h"
#include "ppu.h"
#include "apu.h"

using namespace std;

void renderFrame(SDL_Renderer* renderer, SDL_Texture* screenTex, uint32_t* frameBuffer);

int main(int argc, char* argv[])
{
    int scale = 3;

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        std::cout << "Failed to initialize the SDL2 library\n";
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow("SDL2 Window",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        256 * scale, 240 * scale,
        SDL_WINDOW_SHOWN);

    if (!window)
    {
        std::cout << "Failed to create window\n";
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 
                                                SDL_RENDERER_ACCELERATED | 
                                                SDL_RENDERER_PRESENTVSYNC);

    if (!renderer)
    {
        std::cout << "Failed to create renderer\n";
        std::cout << "SDL2 Error: " << SDL_GetError() << "\n";
        return -1;
	}

    SDL_Texture* screenTex = SDL_CreateTexture(renderer,
                                               SDL_PIXELFORMAT_RGBA8888,
                                               SDL_TEXTUREACCESS_STREAMING,
                                               256, 240);

    if (!screenTex)
    {
        std::cout << "Failed to create texture\n";
        std::cout << "SDL2 Error: " << SDL_GetError() << "\n";
		return -1;
    }

    bool keep_window_open = true;

	std::array<uint32_t, 256 * 240> frameBuffer;

    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 256; x++) {
            frameBuffer[y * 256 + x] = 0xFF0000FF; // Solid red
        }
    }

    // NES COMPONENTS
    Cartridge cartridge;

    if (!cartridge.loadROM("../ROMS/SMB.nes"))
    {
        std::cout << "ROM not loaded.\n";
        return -1;
    }

    PPU ppu(&cartridge);
    //APU apu;

    Memory memory(&cartridge, &ppu);
    CPU cpu(&memory, &ppu);

    while (keep_window_open)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e) > 0)
        {
            switch (e.type)
            {
                case SDL_QUIT:
                    keep_window_open = false;
                    break;
            }
        }

        cpu.step();

        if (ppu.getNMI())
        {
            cpu.handleNMI();
        }

        if (ppu.isFrameComplete())
        {
            renderFrame(renderer, screenTex, ppu.getFrameBuffer());
            //renderFrame(renderer, screenTex, const_cast<uint32_t*>(frameBuffer.data()));
            ppu.resetFrameComplete();
        }
    }

    SDL_DestroyTexture(screenTex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

void renderFrame(SDL_Renderer* renderer, SDL_Texture* screenTex, uint32_t* frameBuffer)
{
    SDL_UpdateTexture(screenTex, nullptr, frameBuffer, 256 * sizeof(uint32_t));
	SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, screenTex, nullptr, nullptr);
	SDL_RenderPresent(renderer);
}