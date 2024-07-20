#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>


/* SDL (Simple Direct Media Layer) is a library that abstracts multimedia hardware components 
    This allows for easy access to generating windows, sound effects, rendering images, etc
*/

#include "SDL.h"


// type alias for a struct containing a pointer attribute of type SDL_Window which we call "sdl_t"
// SDL Container Object
typedef struct {
      SDL_Window *window;
      SDL_Renderer *renderer;
} sdl_t;

// Emulator Config object
typedef struct {
    uint32_t window_width;  // SDL window width
    uint32_t window_height; // SDL window height
    uint32_t fg_color;      // Foreground Color RGBA8888 (bits)
    uint32_t bg_color;      // Background Color RGBA8888 (bits)
    uint32_t scale_factor;  // Amount to scale a CHIP8 pixel by ... e.g 20x will be a 20x larger window
    bool pixel_outlines;    // Draw pixel outlines yes/no 
} config_t;

//Emulator states
typedef enum {
    QUIT = 0,
    RUNNING,
    PAUSED,
} emulator_state_t;


// CHIP8 instruction format
typedef struct{
    uint16_t opcode;    
    uint16_t NNN;       // 12-bit address/constant
    uint8_t NN;         // 8-bit constant
    uint8_t N;          // 4-bit constant
    uint8_t X;          // 4-bit register identifier
    uint8_t Y;          // 4-bit register identifier
    
} instruction_t;


// CHIP8 Machine object
typedef struct {

    emulator_state_t state;
    uint8_t ram[4096];  //Memory of the machine is 4096 Bytes
    
    // approaches to display
    
    // uint8_t *display; // display = &ram[0xF00] - &ram[0xFFF]
    bool display[64*32]; // emulate original chip8 resolution pixels


    uint16_t stack[12];        // subroutine stack
    uint16_t *stack_ptr;       // stack pointer
    uint8_t V[16];             // data registers V0-VF
    uint16_t I;                // index register
    uint16_t PC;               // Program Counter
    uint8_t delay_timer;       // decrements at 60hz when >0
    uint8_t sound_timer;       // decrements at 60hz and plays tone when >0
    bool keypad[16];           // hexadecimal keypad 0x0-0xF
    const char *rom_name;      // currently running ROM
    
    instruction_t inst;        // currently executing instruction

} chip8_t;


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
        .bg_color = 0x000000FF, // BLACK
        .scale_factor = 20,     // Default resolution will be 1280x640
        .pixel_outlines = true, // Draw pixel outlines by default
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


// Initialise CHIP8 machine
bool init_chip8(chip8_t *chip8, const char rom_name[]){
    const uint32_t entry_point = 0x200; // CHIP8 ROM will be loaded to 0x200
    const uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };

    // Load font
    memcpy(&chip8->ram[0], font, sizeof(font));


    // Load ROM to chip8 memory

    // Open ROM file
    FILE *rom = fopen(rom_name, "rb");
    if (!rom){
        SDL_Log("ROM File %s is invalid or does not exist\n", rom_name);
        return false;
    }
    

    // get/check rom size 
    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof chip8->ram - entry_point;
    rewind(rom);

    if (rom_size > max_size){
        SDL_Log("ROM File %s is too big! ROM size: %zu, Max size allowed: %zu \n", rom_name, rom_size, max_size);
        return false;
    }


    if (fread(&chip8->ram[entry_point], rom_size,1, rom ) !=1) {
        SDL_Log("Could not read ROM file %s into CHIP8 memory\n", rom_name);
        return false;
    }

    fclose(rom);

    //set chip8 machine defaults
    chip8->state = RUNNING;     // Default machine state to on/running 
    chip8->PC = entry_point;    // start pc at ROM entry point
    chip8->rom_name = rom_name;
    chip8->stack_ptr = &chip8->stack[0];

    return true; // success
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
void update_screen(const sdl_t sdl, const config_t config, const chip8_t chip8){
    SDL_Rect rect = {.x = 0, .y = 0, .w = config.scale_factor, .h = config.scale_factor};
    
    //grab colour values to draw
    const uint8_t bg_r = (config.bg_color >> 24) & 0xFF; 
    const uint8_t bg_g = (config.bg_color >> 16) & 0xFF;
    const uint8_t bg_b = (config.bg_color >> 8 ) & 0xFF;
    const uint8_t bg_a = (config.bg_color >> 0 ) & 0xFF;

    const uint8_t fg_r = (config.fg_color >> 24) & 0xFF; 
    const uint8_t fg_g = (config.fg_color >> 16) & 0xFF;
    const uint8_t fg_b = (config.fg_color >> 8 ) & 0xFF;
    const uint8_t fg_a = (config.fg_color >> 0 ) & 0xFF;

    // loop through display pixels, draw a rectangle per pixel to the SDL Window
    for (uint32_t i =0 ; i < sizeof chip8.display; i++){
        // translate 1D index i value to 2D XY coordinates
        // X = i % window_width
        // Y = i / window_width
        rect.x = (i % config.window_width) * config.scale_factor;
        rect.y = (i / config.window_width) * config.scale_factor;

        if (chip8.display[i]){
            // If the pixel is on, draw foreground color
            SDL_SetRenderDrawColor(sdl.renderer, fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);

            // if user requested drawing pixel outlines, draw those here
            if (config.pixel_outlines){
                SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
                SDL_RenderDrawRect(sdl.renderer, &rect);

            }


        }else{
            //Pixel is off, draw background color
            SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        }


    }
    
    SDL_RenderPresent(sdl.renderer);

    
}

// Handle user input
void handle_input(chip8_t *chip8){
    SDL_Event event;
    
    while(SDL_PollEvent(&event)) {
        switch (event.type){
            case SDL_QUIT:
                // Exit window; End program
                chip8->state = QUIT; // Will exit main emulator loop
                return;
            
            case SDL_KEYDOWN:

                switch(event.key.keysym.sym){
                    case SDLK_ESCAPE:
                        //escape key; exit window & end program
                        chip8->state = QUIT;
                        return;
                    case SDLK_SPACE:
                        // Space bar - pause emulator
                        if (chip8->state == RUNNING){
                            chip8->state = PAUSED; //pause
                            puts("=====PAUSED=====");

                        }else{
                            chip8->state = RUNNING; // resume
                        }
                        return;

                    default:
                        break;
                }
                break;

            case SDL_KEYUP:  
                break;

            default:
                break;
         }
    }
}

#ifdef DEBUG
void print_debug_info(chip8_t *chip8){
    printf("Address: 0x%04X, Opcode: 0x%04x Desc: ",chip8->PC-2, chip8->inst.opcode);
    switch ((chip8->inst.opcode >> 12) & 0x0F){ // get top 4 MSBs
        case 0x00:
            if ( chip8->inst.NN == 0xE0){
                //0x00E0: clear screen
                printf("Clear screen\n");
                memset(&chip8->display[0], false, sizeof chip8->display);
            } else if (chip8->inst.NN == 0xEE){
                // 0x0EEE: return from subroutine
                // Set PC to  last address on subroutine stack ("pop" it off the stack )
                //  so next opcode is retrieved from that address
                printf("Return from subroutine to address 0x%04X\n", *(chip8->stack_ptr-1));
                chip8->PC = *--chip8->stack_ptr;
            }else{
                printf("Unimplemented opcode\n");
            }
            break;
        case 0x01:
            // 0x1NNN: Jumps to address NNN
            printf("Jump to address NNN (0x%04X)\n", chip8->inst.NNN);
            break;
        case 0x02:
            // 0x2NNN: Call subroutine at NNN
            // store current address to return to on subroutine stack ("push" it on the stack)
            //   and set PC to subroutine address so next opcode is gotten from there
            *chip8->stack_ptr++ = chip8->PC; 
            chip8->PC = chip8->inst.NNN;
            break;
        case 0x06:
            // 0x06NN: Set register VX to NN
            printf("Set register V%X = NN (0x%2X)\n",
            chip8->inst.X, chip8->inst.NN);
            break;
        case 0x07:
            // 0x07XNN: Set register VX += NN
            printf("Set register V%X (0x%02X) += NN (%0x2X). Result 0x%02XN \n",
            chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN,
            chip8->V[chip8->inst.X] +chip8->inst.NN );
            break;
        case 0x0A:
            // 0x0ANNN: Set index register I to NNN
            printf("Set I to NNN (0x%04X)\n", chip8->inst.NNN);
            break;
        case 0x0D:
            // 0xDXYN: Draw N height sprite at coordinates X,Y; read from mem location I;
            // screen pixels are xor'd with sprite bits 
            // VF (carry flag) is set if any screen pixels are set off; useful for 
            // collision detections and other stuff
            printf("Draw N (%u) height sprite at coords V%X (0x%02X), V%X (0x%02X)"
             "from memory location I (%04X). Set VF = 1 if any pixels are turned off\n",
             chip8->inst.N, chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y], chip8->I );
            break;
        default:
            printf("Unimplemented opcode\n");
            break; //unimplemented or invalid opcode
    }

}
#endif


// Emulate 1 CHIP8 instruction
void emulate_instruction(chip8_t *chip8, config_t config){
    // since x86 is little endian and chip 8 is big endian
    // get next opcode from ROM/ram
    chip8->inst.opcode = (chip8->ram[chip8->PC] << 8) | chip8 -> ram[chip8->PC+1];
    chip8->PC +=2; // preincrement PC for next opcode -- 2 bytes 

    // fill out current instruction format
    // DXYN
    chip8->inst.NNN = chip8->inst.opcode & 0x0FFF;
    chip8->inst.NN = chip8->inst.opcode & 0x0FF;
    chip8->inst.N = chip8->inst.opcode & 0x0F;
    chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F; // right bit shift by 8 to get bits 9-12 
    chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F; // right bit shift by 4 to get bits 5-8

    #ifdef DEBUG
        print_debug_info(chip8);
    #endif


    //emulate opcode 
    switch ((chip8->inst.opcode >> 12) & 0x0F){ // get top 4 MSBs
        case 0x00:
            if ( chip8->inst.NN == 0xE0){
                //0x00E0: clear screen
                memset(&chip8->display[0], false, sizeof chip8->display);
            } else if (chip8->inst.NN == 0xEE){
                // 0x0EEE: return from subroutine
                // Set PC to  last address on subroutine stack ("pop" it off the stack )
                //  so next opcode is retrieved from that address
                chip8->PC = *--chip8->stack_ptr;
            }
            break;
        case 0x01:
            // 0x1NNN: Jumps to address NNN
            chip8->PC = chip8->inst.NNN; // set PC so that next opcode is from NNN
            break;
        case 0x02:
            // 0x2NNN: Call subroutine at NNN
            // store current address to return to on subroutine stack ("push" it on the stack)
            //   and set PC to subroutine address so next opcode is gotten from there
            *chip8->stack_ptr++ = chip8->PC; 
            chip8->PC = chip8->inst.NNN;
            break;
        case 0x06:
            // 0x06NN: Set register VX to NN
            chip8->V[chip8->inst.X] = chip8->inst.NN;
            break;
        case 0x07:
            // 0x07XNN: Set register VX += NN
            chip8->V[chip8->inst.X] += chip8->inst.NN;
            break;
        case 0x0A:
            // 0x0ANNN: Set index register I to NNN
            chip8->I  = chip8->inst.NNN;
            break;
        case 0x0D:
            // 0xDXYN: Draw N height sprite at coordinates X,Y; read from mem location I;
            // screen pixels are xor'd with sprite bits 
            // VF (carry flag) is set if any screen pixels are set off; useful for 
            // collision detections and other stuff

            uint8_t X_coord = chip8->V[chip8->inst.X] % config.window_width; 
            uint8_t Y_coord = chip8->V[chip8->inst.Y] % config.window_height;
            const uint8_t orig_X = X_coord; // original X value


            chip8->V[0xF] = 0; // initialise carry flag to 0

            // loop over all N rows of the sprite
            for ( uint8_t i = 0; i< chip8->inst.N;i++){
                // get next byte/ row of sprite data
                const uint8_t sprite_data = chip8->ram[chip8->I + i];
                X_coord = orig_X; // reset X for next row to draw

                for (int8_t j = 7; j >= 0; j--){
                    bool *pixel = &chip8->display[Y_coord * config.window_width + X_coord];
                    const bool sprite_bit = sprite_data & ( 1 << j );
                    // if sprite pixel/bit is on and display pixel is on, set carry flag
                    if ( sprite_bit && *pixel) {

                        chip8->V[0xF] = 1;

                    }

                    // XOR display pixel with sprite pixel/bit to set it on/off
                    *pixel ^= sprite_bit;

                    // stop drawing if hit right edge of screen
                    if(++X_coord >= config.window_width) break; 
                }

                // stop drawing entire sprite if hit bottom edge of screen
                if (++Y_coord >= config.window_height) break;
            } 

            break;
        default:
            break; //unimplemented or invalid opcode
    }

}




int main(int argc, char **argv){
    // default message usage for args
    if (argc < 2){
        fprintf(stderr, "Usage: %s <rom_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    // initialise emulator configuration/options
    config_t config = {0};
    if (!set_config_from_args(&config, argc, argv)) {exit(EXIT_FAILURE);}


    // Initialise SDL
    sdl_t sdl = {0}; // initialise to 0
    if (!init_sdl(&sdl, config)){exit(EXIT_FAILURE);}

    // Initialise CHIP8 machine
    chip8_t chip8 = {0};
    const char *rom_name = argv[1];
    if (!init_chip8(&chip8, rom_name)) {exit(EXIT_FAILURE);}
    
    // Initial screen clear
    clear_screen(sdl, config);

    //Main emulator loop
    while(chip8.state != QUIT){
        // Handle user_input
        handle_input(&chip8);
        if (chip8.state == PAUSED) continue;

        // Get_time()

        // Emulate CHIP8 instructions
        emulate_instruction(&chip8, config);

        // Get_time() elapsed since last Get_time()
        // Delay for approximately 60hz/60fps
        SDL_Delay(16); // time in ms 
        // Update window with changes
        update_screen(sdl, config, chip8);

    }
    
    //Final cleanup
    final_cleanup(sdl);

    exit(EXIT_SUCCESS);
}