
/*
 * Copyright 2006 by Brian Dominy <brian@oddchange.com>
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

/**
 * \file
 * \brief Common trough logic
 *
 * The trough device is present on all games.  The logic here handles
 * variations such as different number of switches, and different types
 * of kicker coils.
 *
 * The key action performed by the trough callbacks is to update the
 * number of "live balls".  When a ball enters/exits the trough, this
 * count is adjusted.  When live balls goes to zero, this creates the
 * end of ball condition.  When it goes up, it indicates that a ball
 * was successfully added to play.
 */

#include <freewpc.h>
#include <mach/switch.h>
#include <mach/coil.h>


/* For games with an outhole switch.
 *
 * The outhole switch is simple; just trigger the kicker
 * to move the balls into the trough.
 */
DECLARE_SWITCH_DRIVER (sw_trough)
{
	.devno = SW_DEVICE_DECL(DEVNO_TROUGH),
};


#ifdef MACHINE_OUTHOLE_SWITCH

void handle_outhole (void)
{
	while (switch_poll (SW_OUTHOLE))
	{
		sol_pulse (SOL_OUTHOLE);
		task_sleep_sec (2);
	}
	task_exit ();
}

void sw_outhole_handler (void)
{
	if (event_did_follow (any_outlane, center_drain))
	{
		/* drained via outlane */
	}
	else
	{
		/* drained down the center */
		audit_increment (&system_audits.center_drains);
	}

	task_create_gid1 (GID_OUTHOLE_HANDLER, handle_outhole);
}

DECLARE_SWITCH_DRIVER (sw_outhole)
{
	.fn = sw_outhole_handler,
	.flags = SW_IN_TEST,
};

#endif /* MACHINE_OUTHOLE_SWITCH */


#if defined(MACHINE_LAUNCH_SWITCH) && defined(MACHINE_LAUNCH_SOLENOID) && defined(MACHINE_SHOOTER_SWITCH)

void sw_launch_handler (void)
{
	if (switch_poll (MACHINE_SHOOTER_SWITCH))
	{
		sol_pulse (MACHINE_LAUNCH_SOLENOID);
	}
}

DECLARE_SWITCH_DRIVER (sw_launch_button)
{
	.fn = sw_launch_handler,
	.flags = SW_IN_GAME,
};

#endif

void trough_enter (device_t *dev)
{
	device_remove_live ();
}


void trough_kick_attempt (device_t *dev)
{
	/* Wait for any conditions that should delay a trough
	 * kick.
	 *
	 * On autoplunging games, always wait for the plunger
	 * area to clear.
	 */
}


void trough_kick_success (device_t *dev)
{
	device_add_live ();
}

void trough_full (device_t *dev)
{
	db_puts ("Trough is full!\n");
}


device_ops_t trough_ops = {
	.enter = trough_enter,
	.kick_attempt = trough_kick_attempt,
	.kick_success = trough_kick_success,
	.full = trough_full,
};


/*
 * The trough device properties.  Most of this is machine
 * dependent and therefore uses MACHINE_xxx defines heavily.
 */
device_properties_t trough_props = {
	.ops = &trough_ops,
	.name = "TROUGH",
	.sol = MACHINE_BALL_SERVE_SOLENOID,
	.sw_count = MACHINE_TROUGH_SIZE,
	.init_max_count = MACHINE_TROUGH_SIZE,
	.sw = {
#ifdef MACHINE_TROUGH1
		MACHINE_TROUGH1,
#endif
#ifdef MACHINE_TROUGH2
		MACHINE_TROUGH2,
#endif
#ifdef MACHINE_TROUGH3
		MACHINE_TROUGH3,
#endif
#ifdef MACHINE_TROUGH4
		MACHINE_TROUGH4,
#endif
#ifdef MACHINE_TROUGH5
		MACHINE_TROUGH5,
#endif
#ifdef MACHINE_TROUGH6
		MACHINE_TROUGH6,
#endif
	}
};


void trough_init (void)
{
	device_register (DEVNO_TROUGH, &trough_props);
}

