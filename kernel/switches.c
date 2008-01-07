/*
 * Copyright 2006, 2007, 2008 by Brian Dominy <brian@oddchange.com>
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
 * \brief Handle all switch inputs
 */

#include <freewpc.h>
#include <sys/irq.h>

/*
 * An active switch is one which has transitioned and is eligible
 * for servicing, but is undergoing additional debounce logic.
 * This struct tracks the switch number, its state, and the
 * debounce timer.
 *
 * There are a finite number of these objects; when the hardware
 * detects a transition, it will either allocate a new slot or
 * re-use an existing slot for the same switch that did not
 * complete its debounce cycle.
 */
typedef struct
{
	/* The switch number that is pending */
	U8 id : 7;

	/* Nonzero if the switch is pending an inactive->active transition. */
	U8 going_active : 1;
	
	/* The amount of time left before the transition completes. */
	U8 timer;
} pending_switch_t;


/* Define shorthand names for the various arrays of switch bits */
#define switch_raw_bits			switch_bits[AR_RAW]
#define switch_changed_bits	switch_bits[AR_CHANGED]
#define switch_pending_bits	switch_bits[AR_PENDING]
#define switch_queued_bits		switch_bits[AR_QUEUED]
#define switch_latched_bits	switch_bits[AR_LATCHED]

/** The global array of switch bits, all in one place */
__fastram__ U8 switch_bits[NUM_SWITCH_ARRAYS][SWITCH_BITS_SIZE];


#ifdef QUEUE_SWITCHES
/** A list of switches that have triggered but are being debounced
 * further */
#define MAX_QUEUED_SWITCHES 8

pending_switch_t switch_queue[MAX_QUEUED_SWITCHES];
#endif


/** Return the switch table entry for a switch */
const switch_info_t *switch_lookup (U8 sw)
{
	extern const switch_info_t switch_table[];
	return &switch_table[sw];
}


/** Return the lamp associated with a switch */
U8 switch_lookup_lamp (const switchnum_t sw)
{
	U8 lamp;
	const switch_info_t *swinfo = switch_lookup (sw);
	lamp = swinfo->lamp;
	return lamp;
}


/** Initialize the switch subsystem */
void switch_init (void)
{
	/* Clear the whole bit buffers */
	memset ((U8 *)&switch_bits[0][0], 0, sizeof (switch_bits));

	/* Initialize the raw state based on mach_opto_mask,
	 * so that the optos don't all trigger at initialization. */
	memcpy (switch_raw_bits, mach_opto_mask, SWITCH_BITS_SIZE);
	memcpy (switch_latched_bits, mach_opto_mask, SWITCH_BITS_SIZE);
}


/** Detect row / column shorts */
void switch_short_detect (void)
{
	U8 n;
	U8 row = 0;

	/* TODO : If any row/column is all 1s, then that row/column is in 
	error and should be ignored until the condition clears. */

	n = switch_bits[AR_RAW][0] & switch_bits[AR_RAW][1] &
		switch_bits[AR_RAW][2] & switch_bits[AR_RAW][3] &
		switch_bits[AR_RAW][4] & switch_bits[AR_RAW][5] &
		switch_bits[AR_RAW][6] & switch_bits[AR_RAW][7];
	/* Each bit in 'n' represents a row that is shorted. */
	while (n != 0)
	{
		if (n & 1)
		{
			dbprintf ("Row %d short detected\n", row);
		}
		n >>= 1;
		row++;
	}

	for (n = 0; n < 8; n++)
	{
		if (switch_bits[AR_RAW][n] == ~mach_opto_mask[n])
		{
			/* The nth column is shorted. */
			dbprintf ("Column %d short detected\n", row);
		}
	}
}


extern inline void switch_rtt_common (void)
{
#if (MACHINE_PIC == 1)
	/* Before any switch data can be accessed on a WPC-S
	 * or WPC95 machine, we need to poll the PIC and see
	 * if the unlock code must be sent to it. */
	U8 unlocked;

	/* Read the status to see if the matrix is still unlocked. */
	wpc_write_pic (WPC_PIC_COUNTER);
	noop ();
	noop ();
	noop ();
	unlocked = wpc_read_pic ();

	if (!unlocked)
	{
		/* We need to unlock it again. */
		extern U8 pic_unlock_code[3];

		wpc_write_pic (WPC_PIC_UNLOCK);
		wpc_write_pic (pic_unlock_code[0]);
		wpc_write_pic (pic_unlock_code[1]);
		wpc_write_pic (pic_unlock_code[2]);
	}
#endif /* MACHINE_PIC */
}


/** Poll a single switch column.
 * Column 0 corresponds to the cabinet switches.
 * Columns 1-8 refer to the playfield columns.
 * It is assumed that columns are polled in order, as
 * during the read of column N, the strobe is set so that
 * the next read will come from column N+1.
 */
extern inline void switch_rowpoll (const U8 col)
{
	register U8 delta __areg__;

	/*
	 * (Optimization: by loading the value into 'delta' first,
	 * it will already be in the correct register needed when
	 * computing delta below.)
	 */
	if (col == 0)
		switch_raw_bits[col] = delta = wpc_asic_read (WPC_SW_CABINET_INPUT);

	else if (col <= 8)
#if (MACHINE_PIC == 1)
		switch_raw_bits[col] = delta = wpc_read_pic ();
#else
		switch_raw_bits[col] = delta = wpc_asic_read (WPC_SW_ROW_INPUT);
#endif

	else if (col == 9)
		switch_raw_bits[col] = delta = wpc_read_flippers ();

	/* delta/changed is TRUE when the switch has changed state from the
	 * previous latched value */
	delta = (switch_raw_bits[col] ^ switch_latched_bits[col]);

	/* pending is TRUE when a switch changes state and remains in the new state
	 * for 2 cycles ; this is a quick and dirty debouncing */
	switch_pending_bits[col] = delta & switch_changed_bits[col];
	switch_changed_bits[col] = delta;

	/* Set up the column strobe for the next read (on the next
	 * iteration) */
	if (col < 8)
	{
#if (MACHINE_PIC == 1)
		wpc_write_pic (WPC_PIC_COLUMN (col));
#else
		wpc_asic_write (WPC_SW_COL_STROBE, 1 << col);
#endif
	}
}


/** Return TRUE if the given switch is CLOSED.
 * This now scans the latched values and not the raw bits,
 * so that quick debouncing is considered. */
bool switch_poll (const switchnum_t sw)
{
	return bitarray_test (switch_latched_bits, sw);
}


/** Return TRUE if the given switch is an opto.  Optos
are defined in the opto mask array, which is auto generated from
the machine description. */
bool switch_is_opto (const switchnum_t sw)
{
	return bitarray_test (mach_opto_mask, sw);
}


/** Return TRUE if the given switch is ACTIVE.  This takes into
 * account whether or not the switch is an opto. */
bool switch_poll_logical (const switchnum_t sw)
{
	return switch_poll (sw) ^ switch_is_opto (sw);
}


/** The realtime switch processing.
 * All that is done is to poll all of the switches and store the
 * updated values in memory.  The idle function will come along
 * later, scan them, and do further, more intensive processing.
 *
 * The polling is broken up to do different switch columns on
 * different iterations.  2 columns are polled every 1ms, for
 * an effective full scan every 4ms.  This is the minimum time
 * that a switch must remain steady in order for the software to
 * sense it.  Sometimes in PinMAME, a switch will toggle so
 * rapidly that this isn't seen.  My guess is this is OK for
 * real hardware.
 */

void switch_rtt_0 (void)
{
	switch_rtt_common ();
	switch_rowpoll (0);
	switch_rowpoll (1);
	switch_rowpoll (2);
}

void switch_rtt_1 (void)
{
	switch_rowpoll (3);
	switch_rowpoll (4);
}

void switch_rtt_2 (void)
{
	switch_rowpoll (5);
	switch_rowpoll (6);
}

void switch_rtt_3 (void)
{
	switch_rowpoll (7);
	switch_rowpoll (8);

#if (MACHINE_FLIPTRONIC == 1)
	/* Poll the Fliptronic flipper switches */
	switch_rowpoll (9);
#endif
}


typedef struct
{
	const switch_info_t *swinfo;
	leff_data_t leffdata;
} lamp_pulse_data_t;


/** Task that performs a switch lamp pulse.
 * Some switches are inherently tied to a lamp.  When the switch
 * triggers, the lamp can be automatically flickered.  This is
 * implemented as a pseudo-lamp effect, so the true state of the
 * lamp is not disturbed. */
void switch_lamp_pulse (void)
{
	lamp_pulse_data_t * const cdata = task_current_class_data (lamp_pulse_data_t);	

	/* Although not a true leff, this fools the lamp draw to doing
	 * the right thing. */
	cdata->leffdata.flags = L_SHARED;

	/* If the lamp is already allocated by another lamp effect,
	then don't bother trying to do the pulse. */
	disable_interrupts ();
	if (!lamp_leff2_test_allocated (cdata->swinfo->lamp))
	{
		lamp_leff2_allocate (cdata->swinfo->lamp);

		/* Change the state of the lamp */
		if (lamp_test (cdata->swinfo->lamp))
			leff_off (cdata->swinfo->lamp);
		else
			leff_on (cdata->swinfo->lamp);
		task_sleep (TIME_200MS);
	
		/* Change it back */
		leff_toggle (cdata->swinfo->lamp);
		task_sleep (TIME_200MS);
	
		/* Free the lamp */
		lamp_leff2_free (cdata->swinfo->lamp);
	}
	task_exit ();
}


/*
 * The entry point for processing a switch transition.  It performs
 * some of the common switch handling logic before calling all
 * event handlers.  Then it also performs some common post-processing.
 * This function runs in a separate task context for each switch that
 * needs to be processed.
 */
void switch_sched (void)
{
	const U8 sw = (U8)task_get_arg ();
	const switch_info_t * const swinfo = switch_lookup (sw);

#ifdef DEBUGGER
	dbprintf ("Handling switch ");
	sprintf_far_string (names_of_switches + sw);
	dbprintf1 ();
#ifdef DEBUG_SWITCH_NUMBER
	dbprintf (" (%d) ", sw);
#endif
	dbprintf ("\n");
#endif

#if 0 /* not working */
	/* In test mode, always queue switch closures into the
	 * special switch test queue. */
	if (in_test)
		switch_test_add_queue (sw);
#endif

	/* Don't service switches marked SW_IN_GAME at all, if we're
	 * not presently in a game */
	if ((swinfo->flags & SW_IN_GAME) && !in_game)
	{
		dbprintf ("Not handling switch because not in game\n");
		goto cleanup;
	}

	/* Don't service switches not marked SW_IN_TEST, unless we're
	 * actually in test mode */
	if (!(swinfo->flags & SW_IN_TEST) && in_test)
	{
		dbprintf ("Not handling switch because in test mode\n");
		goto cleanup;
	}

	/* If the switch has an associated lamp, then flicker the lamp when
	 * the switch triggers. */
	if ((swinfo->lamp != 0) && in_live_game)
	{
		task_pid_t tp = task_create_gid (GID_SWITCH_LAMP_PULSE, 
			switch_lamp_pulse);

		lamp_pulse_data_t *cdata = task_init_class_data (tp, lamp_pulse_data_t);
		cdata->swinfo = swinfo;
	}

	/* If we're in a live game and the switch declares a standard
	 * sound, then make it happen. */
	if ((swinfo->sound != 0) && in_live_game)
		sound_send (swinfo->sound);

	/* If the switch declares a processing function, call it.
	 * All functions are in the EVENT_PAGE. */
	if (swinfo->fn)
		callset_pointer_invoke (swinfo->fn);

	/* If a switch is marked SW_PLAYFIELD and we're in a game,
	 * then call the global playfield switch handler and mark
	 * the ball 'in play'.  Don't do the last bit if the switch
	 * specifically doesn't want this to happen.  Also, kick
	 * the ball search timer so that it doesn't expire.
	 */
	if ((swinfo->flags & SW_PLAYFIELD) && in_game)
	{
		callset_invoke (any_pf_switch);
		if (!(swinfo->flags & SW_NOPLAY) && !ball_in_play)
			mark_ball_in_play ();
		ball_search_timer_reset ();
	}

cleanup:
	/* If the switch is part of a device, then let the device
	 * subsystem process the event */
	if (SW_HAS_DEVICE (swinfo))
		device_sw_handler (SW_GET_DEVICE (swinfo));

	/* Debounce period after the switch has been handled.
	TODO : QUEUE_SWITCHES will do this much better */
	if (swinfo->inactive_time == 0)
		task_sleep (TIME_100MS * 1);
	else
		task_sleep (swinfo->inactive_time);

#if 0
	/* This code isn't needed at the moment */
	register bitset p = (bitset)switch_bits[AR_QUEUED];
	register U8 v = sw;
	bitarray_clear (p, v);
#endif

	task_exit ();
}


/** Idle time switch processing.
 * 'Pending switches' are scanned and handlers are spawned for each
 * of them (each is a separate task).
 */
CALLSET_ENTRY (switch, idle)
{
	U8 rawbits, pendbits;
#ifdef QUEUE_SWITCHES
	U8 queued_bits;
#endif
	U8 col;
	extern U8 sys_init_complete;

	/* Prior to system initialization, switches are not serviced.
	Any switch closures during this time continued to be queued up.
	However, at the least, allow the coin door switches to be polled. */
	if (unlikely (sys_init_complete == 0))
	{
		switch_latched_bits[0] = switch_raw_bits[0];
		return;
	}

	for (col = 0; col <= 9; col++) /* TODO : define for 9? */
	{
		/* Atomically get-and-clear the pending switches */
		disable_irq ();
		pendbits = switch_pending_bits[col];
		switch_pending_bits[col] = 0;
		enable_irq ();

		/* Updated latched bits out of IRQ in new scheme, as it just toggles */
		switch_latched_bits[col] ^= pendbits;

		/* Grab the latched bits : 0=open, 1=closed */
		rawbits = switch_latched_bits[col];

		/* Invert for optos: 0=inactive, 1=active */
		rawbits ^= mach_opto_mask[col];

		/* Convert to active level: 0=inactive, 1=active or edge */
		rawbits |= mach_edge_switches[col];

		/* Grab the current set of pending bits, masked with rawbits.
		 * pendbits is only 1 if the switch is marked pending and it
		 * is currently active.  For edge-triggered switches, it is
		 * invoked active or inactive.
		 */

		if (pendbits & rawbits) /* Anything to be done on this column? */
		{
			/* Yes, calculate the switch number for the first row in the column */
			U8 sw = col * 8;

#ifdef QUEUE_SWITCHES
			queued_bits = switch_queued_bits[col];
#endif

			/* Iterate over all rows -- all switches on this column */
			do {
				if (pendbits & 1)
				{
					/* OK, the switch has changed state and is stable. */
#ifdef QUEUE_SWITCHES
					/* TODO : See if we already have entered this switch into
					 * the wait queue.
					 */
					if (queued_bits & 1)
					{
						/* The switch is already queued up.  Decrement its
						wait time, and if it hits zero, then call the handler. */
						/* Also, need to check if the switch transitioned back
						to its old state before its wait time expired.  In that
						case, latched bits should not be updated above, and
						the transition should be aborted with no action */
					}
					else
					{
						/* This switch just transitioned.  Initialize its
						wait time if nonzero, else go ahead and call the
						handler */
					}
#else /* !QUEUE_SWITCHES */
					/* Start a task to process the switch right away */
					task_pid_t tp = task_create_gid (GID_SW_HANDLER, switch_sched);
					task_set_arg (tp, sw);
#endif /* QUEUE_SWITCHES */
				}
				pendbits >>= 1;
#ifdef QUEUE_SWITCHES
				queued_bits >>= 1;
#endif
				sw++;
			} while (pendbits);
		}
	}

	switch_short_detect ();
}


#if 0 /* just for testing */
void bar (TASK_DECL_ARGS(U8 arg1, U8 arg2))
{
	dbprintf ("arg1 = %d\n", arg1);
	dbprintf ("arg2 = %d\n", arg2);
	task_exit ();
}

void foo (void)
{
	task_pid_t tp = task_create_anon (bar);
	task_push_arg (tp, (U8)10);
	task_push_arg (tp, (U8)5);
	task_push_args_done (tp);
}
#endif

