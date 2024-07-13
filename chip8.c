#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>


/* SDL (Simple Direct Media Layer) is a library that abstracts multimedia hardware components 
    This allows for easy access to generating windows, sound effects, rendering images, etc
*/

#include "SDL.h"


// type alias for a struct containing a pointer attribute of type SDL_Window which we call "sdl_t"
typedef struct {
      SDL_Window *window;
      SDL_Renderer *renderer;
} sdl_t;


typedef struct {
    uint32_t window_width;  // SDL window width
    uint32_t window_height; // SDL window height
    uint32_t fg_color;      // Foreground Color RGBA8888 (bits)
    uint32_t bg_color;      // Background Color RGBA8888 (bits)
    uint32_t scale_factor;  // Amount to scale a CHIP8 pixel by ... e.g 20x will be a 20x larger window
} config_t;


//initialise SDL
bool init_sdl(sdl_t *sdl, const config_t config){ // pass in the sdl_t struct as a pointer

    // if the video, audio, or timer components have not been initialised return an error
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0){

        SDL_Log("Could not initialise SDL subsystems! %s\n", SDL_GetError());
        return false;
    }

    // creation of the window
    // set the window attribute of sdl to be the following (i.e initialise the window)
    sdl->window = SDL_CreateWindow("Chip 8 Emulator",
         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
         config.window_width * config.scale_factor,
         config.window_height * config.scale_factor,
          0);

    if (!sdl->window){
        SDL_Log("Could not create SDL Window  %s\n", SDL_GetError());
        return false;
    } 


    // creates the renderer (the thing that can be drawn to)
    sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);

    if (!sdl->renderer){
        SDL_Log("Could not create SDL renderer %s\n", SDL_GetError());
        return false;
    }

    return true;
}

// Setup initial emulator configuration from passed in args
bool set_config_from_args(config_t *config, const int argc, char **argv){
    
    //set defaults
    *config = (config_t){
        .window_width = 64,     // CHIP8 original X resolution
        .window_height = 32,    // CHIP8 original Y resolution
        .fg_color = 0xFFFFFFFF, // WHITE
        .bg_color = 0xFFFF00FF, // YELLOW
        .scale_factor = 20,     // Default resolution will be 1280x640
    };

    //override defaults from args

    for (int i=1; i< argc;i++){
        (void)argv[i]; //prevent compiler error unused 
        // ...
    }
    return true;

}

//  Final cleanup
void final_cleanup(const sdl_t sdl){
    SDL_DestroyRenderer(sdl.renderer);
    SDL_DestroyWindow(sdl.window); // close the window
    SDL_Quit(); //shutdown SDL subsystem
}

// Clear Screen / SDL Window to Background Color
void clear_screen(const sdl_t sdl, const config_t config){
    const uint8_t r = (config.bg_color >> 24) & 0xFF; // right bit shift by 24 bits to get r component 
    const uint8_t g = (config.bg_color >> 16) & 0xFF; // right bit shift by 24 bits to get g component
    const uint8_t b = (config.bg_color >> 8)  & 0xFF; // right bit shift by 24 bits to get b component
    const uint8_t a = (config.bg_color >> 0)  & 0xFF; // right bit shift by 24 bits to get a component
    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    SDL_RenderClear(sdl.renderer);
}

//Update window with any changes
void update_screen(const sdl_t sdl){
    SDL_RenderPresent(sdl.renderer);
}


int main(int argc, char **argv){

    // initialise emulator configuration/options
    config_t config = {0};
    if (!set_config_from_args(&config, argc, argv)) {exit(EXIT_FAILURE);}


    // Initialise SDL
    sdl_t sdl = {0}; // initialise to 0
    if (!init_sdl(&sdl, config)){exit(EXIT_FAILURE);}

    // Initial screen clear
    clear_screen(sdl, config);

    //Main emulator loop
    while(true){
        // Get_time()
        // Emulate CHIP8 instructions
        // Get_time() elapsed since last Get_time()
        // Delay for approximately 60hz/60fps
        SDL_Delay(16); // time in ms 
        // Update window with changes
        update_screen(sdl);

    }
    
    //Final cleanup
    final_cleanup(sdl);

    exit(EXIT_SUCCESS);
}