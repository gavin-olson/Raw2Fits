/* 
   coords_calc.c
    - auxilarity functions for coordinates calculations

   Copyright 2017  Oleg Kutkov <elenbert@gmail.com>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <locale.h>
#include "coords_calc.h"

static void split_sexigesimal_str(const char *str, float (*splitted)[])
{
	int tok_cnt = 0;
	const char s[2] = ":";
	char *token;

	token = strtok((char *)str, s);

	while( token != NULL ) {
		if (tok_cnt >= 3) {
			return;
		}

		(*splitted)[tok_cnt] = atof(token);
		token = strtok(NULL, s);

		tok_cnt++;
	}

	if ((*splitted)[0] < 0) {
		(*splitted)[1] *= -1;
		(*splitted)[2] *= -1;
	}
}

static float parse_sexigesimal_str(const char *str)
{
	float splitted[3];

	split_sexigesimal_str(str, &splitted);

	return splitted[0] + splitted[1] / 60 + splitted[2] / 3600;
}

static float parse_deg_str(const char *str)
{
	return atof(str);
}

float coord_any_to_float_deg(const char *str)
{
	if (strlen(str) == 0) {
		return 0;
	}

	if (strchr(str, ':') != NULL) {
		return parse_sexigesimal_str(str);
	} else if (strchr(str, '.') != NULL) {
		return parse_deg_str(str);
	}

	return 0;
}

inline double frac(const double x)
{
	return x - floor(x);
}

void deg_to_sexigesimal_str(float deg, char *dst, char hrs)
{
	char buf[15] = { 0 };	

	int hour = (int) deg;

	if (hrs) {
		hour /= 15;
	}

	int min = (int) ((deg - hour) * 60);

	double min_frac = (double)min / 60;
	double sec = (deg - hour - min_frac) * 3600;

	char *saved_locale, *old_locale = setlocale (LC_NUMERIC, NULL);

	saved_locale = strdup(old_locale);

	if (saved_locale == NULL) {
		perror("Unable get current locale, probably out of memory.\n");
		return;
	}

	if (min < 0) {
		min *= -1;
	}

	if (sec < 0) {
		sec *= -1;
	}

	setlocale(LC_NUMERIC, "en_US");

	snprintf(buf, 14, "%i:%i:%.5f", hour, min, sec);
	strcpy(dst, buf);

	setlocale(LC_NUMERIC, saved_locale);
	free(saved_locale);
}

double coordinates_to_deg(short hour, short min, short sec, short msec, char hrs)
{
	double tmp = (hour > 0 ? hour : (hour * -1) ) + ((double)min) / 60 + ((double)sec + (double)msec / 1000) / 3600;

	if (hrs) {
		tmp *= 15;
	}

	return (hour > 0 ? tmp : (tmp * -1));
}

void coordinates_to_sexigesimal_str(short hour, short min, short sec, short msec, char *dst)
{
	snprintf(dst, 14, "%i:%i:%i.%i", hour, min, sec, msec);
}

void sexigesimal_str_to_coords(const char *str, short *hour, short *min, short *sec, short *msec)
{
	float splitted[3];

	split_sexigesimal_str(str, &splitted);

	*hour = splitted[0];
	*min = splitted[1];
	*sec = splitted[2];

	if (*min < 0) {
		*min *= -1;
	}

	if (*sec < 0) {
		*sec *= -1;
	}

	*msec = (short)((splitted[2] - *sec) * 1000);
}

