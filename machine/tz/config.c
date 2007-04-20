/*
 * Copyright 2006, 2007 by Brian Dominy <brian@oddchange.com>
 *
 * This file is part of FreeWPC.
 *
 * FreeWPC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * FreeWPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with FreeWPC; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <freewpc.h>

/** Filename: mach/config.c
 * Machine-specific functions.  These are all callbacks
 * from the main kernel code to handle various events
 * in game-specific ways.
 */

U8 faster_quote_given;

const audio_track_t bonus_music_track = {
	.prio = PRI_BONUS,
	.code = MUS_BONUS_START,
};


CALLSET_ENTRY (tz, start_ball)
{
	faster_quote_given = 0;
}


CALLSET_ENTRY (tz, add_player)
{
#ifdef CONFIG_TZONE_IP
	if (num_players > 1)
		sound_send (SND_PLAYER_ONE + num_players - 1);
#endif
}


CALLSET_ENTRY (tz, bonus)
{
	bg_music_start (&bonus_music_track);
	task_sleep_sec (1);
	deff_start (DEFF_BONUS);
	leff_start (LEFF_BONUS);
	task_sleep_sec (1);
	while (deff_get_active () == DEFF_BONUS)
		task_sleep (TIME_33MS);
	leff_stop (LEFF_BONUS);
	bg_music_stop (&bonus_music_track);
}


CALLSET_ENTRY (tz, tilt)
{
	sound_send (SND_TILT);
	task_sleep_sec (3);
	sound_send (SND_OH_NO);
}


CALLSET_ENTRY (tz, tilt_warning)
{
	sound_send (SND_TILT_WARNING);
}


CALLSET_ENTRY (tz, start_without_credits)
{
	sound_send (SND_GREEEED);
}


CALLSET_ENTRY (tz, timed_game_tick)
{
#ifdef CONFIG_TIMED_GAME
	if (!in_live_game || in_bonus)
		return;
	switch (timed_game_timer)
	{
		case 10: 
			if (faster_quote_given == 0)
				sound_send (SND_FASTER); 
			faster_quote_given = 1;
			break;
		case 5: sound_send (SND_FIVE); break;
		case 4: sound_send (SND_FOUR); break;
		case 3: sound_send (SND_THREE); break;
		case 2: sound_send (SND_TWO); break;
		case 1: sound_send (SND_ONE); break;
		default: break;
	}
#endif
}

