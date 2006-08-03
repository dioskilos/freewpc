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

#include <freewpc.h>

/*
 * This module implements a simple random number generator.
 * This generator does not satisfy the properties required for a really
 * good generator, but is adequate for what pinball machines need.
 *
 * There are 3 independent components to each random number:
 * 1. a linear congruential component, derived from the relation
 * Xn+1 = (A(Xn) + C) mod M, where A=33, C=1, and M=255.  This is
 * fairly easy to do with shifts and adds.
 * 2. a timing component, based on the number of IRQs asserted
 * 3. a secondary timing component, skewed to #2, based on the number
 * of FIRQs asserted.  IRQ is regular but FIRQ is not.
 */


/** The seed for the linear congruential component of the random numbers. */
U8 random_cong_seed;



/**
 * Return a new random number from 0-255.
 */
U8
random (void)
{
	register U8 r;
	extern U8 dmd_page_flip_count;

	r = random_cong_seed << 5;
	r += random_cong_seed;
	r++;
	random_cong_seed = r;
	r ^= irq_count;
	r ^= dmd_page_flip_count;
	return r;
}


/**
 * Return a new random number from 0 to (N-1).
 */
U8
random_scaled (U8 N)
{
	return (random () * (U16)N) >> 8;
}


/**
 * Tweak the random number seed slightly.
 */
void
random_reseed (void)
{
	random_cong_seed++;
}


/**
 * Reseed the random number generator during system idle time.
 */
CALLSET_ENTRY (random, idle)
{
	random_reseed ();
}


/**
 * Initialize the random number generator.
 */
void
random_init (void)
{
	random_cong_seed = 1;
}

