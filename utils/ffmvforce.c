/*
 * Tests the force feedback driver
 * Opens a window. When the user clicks in the window, a force effect
 * is generated according to the position of the mouse.
 * This program needs the SDL library (http://www.libsdl.org)
 * Copyright 2001 Johann Deneux <deneux@ifrance.com>
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 *
 * You can contact the author by email at this address:
 * Johann Deneux <deneux@ifrance.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <linux/input.h>
#include <SDL.h>

#define BIT(x) (1<<(x))
#define	WIN_W	400
#define WIN_H	400
#define max(a,b)	((a)>(b)?(a):(b))

/* File descriptor of the force feedback /dev entry */
static int ff_fd = -1;
static struct ff_effect effect;

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;

static void welcome()
{
	const char* txt[] = {
"ffmvforce: test orientation of forces",
"Click in the window to generate a force whose direction will be the position",
"of your mouse relatively to the center of the window",
"USE WITH CARE !!! HOLD STRONGLY YOUR WHEEL OR JOYSTICK TO PREVENT DAMAGES",
"To run this program, run it with at least one argument.",
"",
NULL };

	const char** p = txt;

	while (*p) {
		printf("%s\n", *p);
		p++;
	}
}

static void generate_force(int x, int y)
{
	static int first = 1;
	double nx, ny;
	double angle;

	nx = 2*(x-WIN_W/2.0)/WIN_W;
	ny = 2*(y-WIN_H/2.0)/WIN_H;
	angle = atan2(nx, -ny);
printf("mouse: %d %d n: %4.2f %4.2f angle: %4.2f\n", x, y, nx, ny, angle);
	effect.type = FF_CONSTANT;
        effect.u.constant.level = 0x7fff * max(fabs(nx), fabs(ny));
        effect.direction = 0x8000 * (angle + M_PI)/M_PI;
printf("level: %04x direction: %04x\n", (unsigned int)effect.u.constant.level, (unsigned int)effect.direction);
        effect.u.constant.envelope.attack_length = 0;
        effect.u.constant.envelope.attack_level = 0;
        effect.u.constant.envelope.fade_length = 0;
        effect.u.constant.envelope.fade_level = 0;
        effect.trigger.button = 0;
        effect.trigger.interval = 0;
        effect.replay.length = 0xffff;
        effect.replay.delay = 0;

	if (first) {
		effect.id = -1;
	}

        if (ioctl(ff_fd, EVIOCSFF, &effect) < 0) {
/* If updates are sent to frequently, they can be refused */
        }

	/* If first time, start to play the effect */
	if (first) {
		struct input_event play;
		play.type = EV_FF;
		play.code = effect.id;
		play.value = 1;

		if (write(ff_fd, (const void*) &play, sizeof(play)) == -1) {
			perror("Play effect");
			exit(1);
		}
	}

	first = 0;
}

static void shutdown()
{
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	SDL_Quit();
	if (ff_fd >= 0) {
		close(ff_fd);
	}
}

int main(int argc, char** argv)
{
	const char * dev_name = "/dev/input/event0";
	Uint32 ticks, timeout, period = 200;
	Sint32 x = WIN_W / 2, y = WIN_H / 2;
	Uint32 state = 0;

	welcome();
	if (argc <= 1) return 0;

	/* Parse parameters */
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--help") == 0) {
			printf("Usage: %s /dev/input/eventXX [-u update frequency in HZ]\n", argv[0]);
			printf("Generates constant force effects depending on the position of the mouse\n");
			exit(1);
		}
		else if (strcmp(argv[i], "-u") == 0) {
			if (++i >= argc) {
				fprintf(stderr, "Missing update frequency\n");
				exit(1);
			}
			period = 1000.0/atof(argv[i]);
		}
		else {
			dev_name = argv[i];
		}
	}

	/* Initialize SDL */
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
		fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}
	atexit(&shutdown);

	window = SDL_CreateWindow("ffmvforce", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, 0);
	if (!window) {
		fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
		exit(1);
	}

	renderer = SDL_CreateRenderer(window, -1, 0);
	if (!renderer) {
		fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());
		exit(1);
	}
		
	/* Open force feedback device */
	ff_fd = open(dev_name, O_RDWR);
	if (ff_fd == -1) {
                perror("Open device file");
		exit(1);
	}

	ticks = SDL_GetTicks();
	timeout = ticks + period;

	/* Main loop */
	for (;;) {
		SDL_Event event;

                if (state) {
                        SDL_WaitEventTimeout(&event, period);
                } else {
                        SDL_WaitEvent(&event);
                }

                do {
			if (event.type == SDL_QUIT) {
				exit(0);

			} else if (event.type == SDL_KEYDOWN) {
				switch(event.key.keysym.sym) {
				case SDLK_ESCAPE:
				case SDLK_q:
					exit(0);
				default:
					break;
				}

			} else if (event.type == SDL_MOUSEBUTTONDOWN) {
				state = event.button.state;
				x = event.button.x;
				y = event.button.y;

			} else if (event.type == SDL_MOUSEMOTION && event.motion.state) {
				state = event.motion.state;
				x = event.motion.x;
				y = event.motion.y;
			}
		} while(SDL_PollEvent(&event));

		ticks = SDL_GetTicks();
		if (state && SDL_TICKS_PASSED(ticks, timeout)) {
			timeout = ticks + period;
			generate_force(x, y);
			state = 0;
		}

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer);
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
		SDL_RenderDrawLine(renderer, 0, WIN_H / 2, WIN_W, WIN_H / 2);
		SDL_RenderDrawLine(renderer, WIN_W / 2, 0, WIN_W / 2, WIN_H);
		SDL_RenderDrawLine(renderer, WIN_W / 2, WIN_H / 2, x, y);
		SDL_RenderPresent(renderer);
	}

	exit(0);
}
