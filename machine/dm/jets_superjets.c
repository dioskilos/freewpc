/*
 * demolition man
 * jets_superjets.c
 *
 * written by James Cardona
 *
 * Location Description:
 * Jet Bumpers: There are two regular jet bumpers and a top slingshot
 * which scores the same as a jet bumper. They advance the
 * Retina Scan value. The left jet bumper can also be hit from
 * the center ramp -- it acts as the right wall of the center ramp.
 *
 * Scoring Description: (original game)
 * The Jet Bumpers increase the Retina Value.(eyeball)
 * It starts at 5M and goes up 100K per jet.
 * There are two Jet Bumpers and one slingshot ... the slingshot counts as a jet bumper.
 * Jet Bumpers award 250k a Pop.
 * superjets mode award 1 million each
 *
 * Scoring Description: (my rules)
 * same as above except
 * superjets mode award 2 million each
 * successive modes award more
 *
 * estimate of average superjets mode score: 20 million to 80 million
 * estimate of average jets score: 2.5 million
 *
 */

#include <freewpc.h>
#include "dm/global_constants.h"

//constants
const U8 JETS_EASY_GOAL = 25;
const U8 JETS_PREDEFINED_GOAL_INCREMENT = 1;
const U8 JETS_GOAL_STEP = 15;
const U8 JETS_GOAL_MAX = 150;
const U8 SUPERJETS_GOAL_MAX = 100;
const U8 SUPERJETS_EASY_GOAL = 20;
const U8 SUPERJETS_GOAL_STEP = 10;
score_t superjets_mode_score;

//local variables
U8 			jet_shots_made;//external ref to eyeball_explode.c
U8 			jet_goal;

U8 			super_jet_shots_made;
U8 			super_jet_goal;
U8 			superjets_modes_achieved; // number of times we entered the mode
U8 			superjets_SoundCounter;
__boolean 	super_jets_activated;

//external variables

//prototypes
void jet_flasher(void);
void jets_effect_deff(void);
void superjets_effect_deff(void);
void jet_goal_reset(void);
void jet_goal_award(void);
void super_jet_goal_award(void);
void player_jet_goal_reset(void);

/****************************************************************************
 * initialize  and exit
 ***************************************************************************/
void jet_goal_reset (void) {
	jet_shots_made = 0;
	super_jet_shots_made = 0;
	super_jets_activated = FALSE;
	superjets_SoundCounter = 0;
	}

void player_jet_goal_reset  (void) {
superjets_modes_achieved = 0;
jet_goal = JETS_EASY_GOAL;
super_jet_goal = SUPERJETS_EASY_GOAL;
jet_goal_reset();
}

CALLSET_ENTRY (jets_superjets, start_player) {  player_jet_goal_reset (); }
CALLSET_ENTRY (jets_superjets, start_ball) { jet_goal_reset (); }



/****************************************************************************
 * body
 ***************************************************************************/

void jet_goal_award (void) {
	sound_start (ST_SAMPLE, EXPLOSION, SL_2S, PRI_GAME_QUICK5);
	jet_shots_made = 0;
	score(SC_1M);//only once
	if (jet_goal < JETS_GOAL_MAX)  jet_goal += JETS_GOAL_STEP;
	}

void super_jet_goal_award (void) {
	sound_start (ST_SPEECH, SPCH_SUPERJETS_COMPLETED, SL_5S, PRI_GAME_QUICK5);
	jet_shots_made = 0;
	super_jet_shots_made = 0;
	//score higher if mode done more than once
	switch (superjets_modes_achieved) {
		case 0: 	break; //never entered mode
		case 1:  { score (SC_40M);  score_add (superjets_mode_score, score_table[SC_40M]); break;}
		case 2:  { score (SC_45M);  score_add (superjets_mode_score, score_table[SC_45M]); break;}
		case 3:  { score (SC_50M);  score_add (superjets_mode_score, score_table[SC_50M]); break;}
		case 4:	 { score (SC_55M);  score_add (superjets_mode_score, score_table[SC_55M]); break;}
		default: { score (SC_60M);  score_add (superjets_mode_score, score_table[SC_60M]); break;}
		}//end of switch
	super_jets_activated = FALSE;
	if (super_jet_goal < SUPERJETS_GOAL_MAX)  super_jet_goal += SUPERJETS_GOAL_STEP;
	//TODO: deff
	//return to normal music
	sound_start (ST_MUSIC, MUS_BG, 0, SP_NORMAL);
	}


CALLSET_ENTRY (jets_superjets, sw_jet) {
	if (super_jets_activated){
		++super_jet_shots_made;
		superjets_SoundCounter = random_scaled(3);//from kernal/random.c - pick number from 0 to 2
		if ( (superjets_SoundCounter) == 0 ) 		sound_start (ST_SPEECH, SPCH_DULCH, SL_2S, PRI_GAME_QUICK5);
		else if ( (superjets_SoundCounter) == 1 ) 	sound_start (ST_SPEECH, SPCH_WOOH, SL_2S, PRI_GAME_QUICK5);
		else 										sound_start (ST_SPEECH, SPCH_WOW, SL_2S, PRI_GAME_QUICK5);

		if (super_jet_shots_made == super_jet_goal)  super_jet_goal_award();

		// deff must be listed in md file or will crash here
			deff_start (DEFF_SUPERJETS_EFFECT);//under /kernel/deff.c
		//score higher if mode done more than once
		switch (superjets_modes_achieved) {
			case 0: 	break; //never entered mode
			case 1:  { score (SC_2M);  score_add (superjets_mode_score, score_table[SC_2M]); break;}
			case 2:  { score (SC_3M);  score_add (superjets_mode_score, score_table[SC_3M]); break;}
			case 3:  { score (SC_4M);  score_add (superjets_mode_score, score_table[SC_4M]); break;}
			case 4:	 { score (SC_5M);  score_add (superjets_mode_score, score_table[SC_5M]); break;}
			default: { score (SC_5M);  score_add (superjets_mode_score, score_table[SC_5M]); break;}
			}//end of switch
	}
	else {//not in super jets mode
		++jet_shots_made;
		score(SC_250K);
		deff_start (DEFF_JETS_EFFECT);//under /kernel/deff.c
		if (jet_shots_made == jet_goal)  jet_goal_award ();//sound played in call
		//TODO: do we want a sound here?
		//	else
	//		sound_start (ST_SAMPLE, EXPLOSION, SL_1S, PRI_GAME_QUICK5);
		}
	task_create_gid1 (GID_JET_FLASHER, jet_flasher);
	}



//claw switch starts superjets mode
CALLSET_ENTRY (jets_superjets, sw_claw_super_jets) {
	super_jets_activated = TRUE;
	++superjets_modes_achieved;
	score_zero (superjets_mode_score);
	music_set (MUS_MD_SUPERJETS); //from sound_effect.c
	sound_start (ST_SAMPLE, SPCH_SUPERJETS_ACTIVATED, SL_5S, PRI_GAME_QUICK5);
	//flash lamp for a time
	lamp_tristate_flash(LM_CLAW_SUPER_JETS);
	task_sleep (TIME_500MS);
	lamp_tristate_off(LM_CLAW_SUPER_JETS);
	}




/****************************************************************************
 * DMD display and sound effects
 ****************************************************************************/
//LIGHTING EFFECTS
void jet_flasher (void) {
	flasher_pulse (FLASH_JETS_FLASHER);//FLASH followed by name of flasher in caps
	flasher_pulse (FLASH_JETS_FLASHER);
	flasher_pulse (FLASH_JETS_FLASHER);
	task_exit ();
	}

//DMD DISPLAY EFFECTS
void jets_effect_deff(void) {
	dmd_alloc_low_clean ();
	if (jet_shots_made <= 1) 	sprintf ("%d JET", jet_shots_made);
	else sprintf ("%d JETS", jet_shots_made);
	font_render_string_center (&font_fixed10, DMD_BIG_CX_Top, DMD_BIG_CY_Top, sprintf_buffer);

	sprintf ("%d FOR BONUS", jet_goal);
	font_render_string_center (&font_fixed6, DMD_BIG_CX_Bot, DMD_BIG_CY_Bot, sprintf_buffer);
	dmd_show_low ();
	task_sleep_sec (2);
	deff_exit ();
	}//end of jets_effect_deff



void superjets_effect_deff(void) {
	dmd_alloc_low_clean ();
	sprintf ("%d SUPERJETS", super_jet_shots_made);
	font_render_string_center (&font_fixed10, DMD_BIG_CX_Top, DMD_BIG_CY_Top, sprintf_buffer);

	sprintf ("%d FOR BONUS", super_jet_goal);
	font_render_string_center (&font_fixed6, DMD_BIG_CX_Bot, DMD_BIG_CY_Bot, sprintf_buffer);
	dmd_show_low ();
	task_sleep_sec (2);
	deff_exit ();
	}//end of superjets_effect_deff



/****************************************************************************
 * status display
 *
 * called from common/status.c automatically whenever either flipper button
 * is held for 4 seconds or longer.  since called by callset, order of
 * various status reports will be random depending upon call stack
****************************************************************************/
//ALLSET_ENTRY (superjets, status_report){
//		if (super_jets_activated) sprintf ("SUPER JETS");
//		font_render_string_left (&font_fixed10, 1, 1, sprintf_buffer);

//	sprintf ("%d superjets modes completed", superjets_modes_achieved);
//	font_render_string_center (&font_mono5, 64, 7, sprintf_buffer);

//		sprintf ("%d superjets shots made", super_jet_shots_made);
//		font_render_string_center (&font_mono5, 64, 19, sprintf_buffer);
	//deff_exit (); is called at end of calling function - not needed here?
//}//end of function
