#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <stdbool.h>
#include <math.h>

#define MW 32
#define MH 32

int main(void)
{
	initscr();
	noecho();
	keypad(stdscr, true);
	curs_set(false);

	char *map = malloc(MW * MH);
	for (int i = 0; i < MH; i++)
		for (int j = 0; j < MW; j++) {
			if (i == 0 || i == MH - 1 || j == 0 || j == MW - 1) {
				map[i * MW + j] = '#';
				continue;
			}
			if (!(rand() % 10))
				map[i * MW + j] = '#';
			else
				map[i * MW + j] = '.';
		}
	float x = 8.0f;
	float y = 8.0f;
	float dir = 0.0f;
	float fov = M_PI / 4.0f;
	int winw = 10; int winh = 10;
	getmaxyx(stdscr, winh, winw);
	bool quit = false;
	while (!quit) {
		clear();
		for (int i = 0; i < winw; i++) {
			float angle = (dir - (fov / 2.0f)) + ((float)i / (float)winw) * fov;
			//printf("angle: %f\n", angle);
			float dirx = cosf(angle);
			float diry = sinf(angle);
			int hit = 0;
			float d = 0.1f;
			float dlimit = 16.0f;
			while (!hit) {
				int tx = (x + (dirx * d));
				int ty = (y + (diry * d));
				d += 0.1f;
				if (d > dlimit) {
					//d = -1.0f;
					break;
				}
				// out of bound check
				if (tx >= MW || tx < 0 || ty >= MH || ty < 0) {
					//d = -1.0f;
					break;
				}
				if (map[ty * MW + tx] == '#') {
					hit = 1;
				}
			}
			mvprintw(0, 0, "win w: %d, win h: %d", winw, winh);
			if (!hit)
				continue;
			int floordist = (float)((float)winh / 2.0f) - ((float)winh / d);
			int ceildist = winh - floordist;
			for (int j = 0; j < winh; j++) {
				if (j > floordist && j <= ceildist) {
					if (d < 5.0f)
						mvprintw(j, i, "#");
					else if (d >= 5.0f && d < 10.0f)
						mvprintw(j, i, "|");
					else
						mvprintw(j, i, ".");
				}
				else
					mvprintw(j, i, " ");
			}
			mvprintw(1, 0, "x: %f, y: %f", x, y);
			mvprintw(2, 0, "[w][s] to move, [a][d] to rotate");
		}
		refresh();
		char c = getch();
		if (c == 'a')
			dir -= 0.1f;
		else if (c == 'd')
			dir += 0.1f;
		else if (c == 'w') {
			x += 0.1f * cosf(dir);
			y += 0.1f * sinf(dir);
		} else if (c == 's') {
			x -= 0.1f * cosf(dir);
			y -= 0.1f * sinf(dir);
		}
	}
	endwin();

}
