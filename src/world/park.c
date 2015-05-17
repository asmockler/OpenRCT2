/*****************************************************************************
 * Copyright (c) 2014 Ted John
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * This file is part of OpenRCT2.
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include "../addresses.h"
#include "../game.h"
#include "../interface/window.h"
#include "../localisation/localisation.h"
#include "../management/award.h"
#include "../management/finance.h"
#include "../management/marketing.h"
#include "../management/news_item.h"
#include "../management/research.h"
#include "../peep/peep.h"
#include "../ride/ride.h"
#include "../scenario.h"
#include "../world/map.h"
#include "park.h"
#include "sprite.h"

/**
 * In a difficult guest generation scenario, no guests will be generated if over this value.
 */
int _suggestedGuestMaximum;

/**
 * Probability out of 65535, of gaining a new guest per game tick.
 * new guests per second = 40 * (probability / 65535)
 * With a full park rating, non-overpriced entrance fee, less guests than the suggested maximum and four positive awards,
 * approximately 1 guest per second can be generated (+60 guests in one minute).
 */
int _guestGenerationProbability;

int park_is_open()
{
	return (RCT2_GLOBAL(RCT2_ADDRESS_PARK_FLAGS, uint32) & PARK_FLAGS_PARK_OPEN) != 0;
}

/**
 * 
 *  rct2: 0x00667132
 */
void park_init()
{
	int i;

	RCT2_GLOBAL(0x013CA740, uint8) = 0;
	RCT2_GLOBAL(RCT2_ADDRESS_PARK_NAME, uint16) = 777;
	RCT2_GLOBAL(RCT2_ADDRESS_HANDYMAN_COLOUR, uint8) = 28;
	RCT2_GLOBAL(RCT2_ADDRESS_MECHANIC_COLOUR, uint8) = 28;
	RCT2_GLOBAL(RCT2_ADDRESS_SECURITY_COLOUR, uint8) = 28;
	RCT2_GLOBAL(RCT2_ADDRESS_GUESTS_IN_PARK, uint16) = 0;
	RCT2_GLOBAL(0x01357BC8, uint16) = 0;
	RCT2_GLOBAL(RCT2_ADDRESS_GUESTS_HEADING_FOR_PARK, uint16) = 0;
	RCT2_GLOBAL(0x013573FE, uint16) = 0;
	RCT2_GLOBAL(RCT2_ADDRESS_CURRENT_PARK_RATING, uint16) = 0;
	_guestGenerationProbability = 0;
	RCT2_GLOBAL(RCT2_TOTAL_RIDE_VALUE, uint16) = 0;
	RCT2_GLOBAL(RCT2_ADDRESS_LAST_RESEARCHED_ITEM_SUBJECT, sint32) = -1;

	for (i = 0; i < 20; i++)
		gMarketingCampaignDaysLeft[i] = 0;

	research_reset_items();
	finance_init();

	for (i = 0; i < 2; i++)
		RCT2_ADDRESS(0x01357404, uint32)[i] = 0;

	for (i = 0; i < 56; i++)
		RCT2_ADDRESS(0x01357BD0, sint32)[i] = -1;

	RCT2_GLOBAL(RCT2_ADDRESS_PARK_ENTRANCE_FEE, money16) = MONEY(10, 00);
	RCT2_GLOBAL(RCT2_ADDRESS_PEEP_SPAWNS, sint16) = -1;
	RCT2_GLOBAL(0x013573F8, sint16) = -1;
	RCT2_GLOBAL(RCT2_ADDRESS_ACTIVE_RESEARCH_TYPES, uint16) = 127;
	RCT2_GLOBAL(RCT2_ADDRESS_CURRENT_RESEARCH_LEVEL, uint8) = 2;

	RCT2_GLOBAL(RCT2_ADDRESS_GUEST_INITIAL_CASH, uint16) = MONEY(50,00); // Cash per guest (average)
	RCT2_GLOBAL(RCT2_ADDRESS_GUEST_INITIAL_HAPPINESS, uint8) = calculate_guest_initial_happiness(50); // 50%
	RCT2_GLOBAL(RCT2_ADDRESS_GUEST_INITIAL_HUNGER, uint8) = 200;
	RCT2_GLOBAL(RCT2_ADDRESS_GUEST_INITIAL_THIRST, uint8) = 200;
	RCT2_GLOBAL(RCT2_ADDRESS_OBJECTIVE_TYPE, uint8) = 1;
	RCT2_GLOBAL(RCT2_ADDRESS_OBJECTIVE_YEAR, uint8) = 4;
	RCT2_GLOBAL(RCT2_ADDRESS_OBJECTIVE_NUM_GUESTS, uint16) = 1000;
	RCT2_GLOBAL(RCT2_ADDRESS_LAND_COST, uint16) = MONEY(90, 00);
	RCT2_GLOBAL(RCT2_ADDRESS_CONSTRUCTION_RIGHTS_COST, uint16) = MONEY(40,00);
	RCT2_GLOBAL(0x01358774, uint16) = 0;
	RCT2_GLOBAL(RCT2_ADDRESS_PARK_FLAGS, uint32) = PARK_FLAGS_NO_MONEY | PARK_FLAGS_SHOW_REAL_GUEST_NAMES;
	park_reset_history();
	finance_reset_history();
	award_reset();

	rct_s6_info *info = (rct_s6_info*)0x0141F570;
	info->name[0] = '\0';
	format_string(info->details, STR_NO_DETAILS_YET, NULL);
}

/**
 * 
 *  rct2: 0x0066729F
 */
void park_reset_history()
{
	int i;
	for (i = 0; i < 32; i++) {
		RCT2_ADDRESS(RCT2_ADDRESS_PARK_RATING_HISTORY, uint8)[i] = 255;
		RCT2_ADDRESS(RCT2_ADDRESS_GUESTS_IN_PARK_HISTORY, uint8)[i] = 255;
	}
}

/**
 * 
 *  rct2: 0x0066A348
 */
int park_calculate_size()
{
	int tiles;
	map_element_iterator it;

	tiles = 0;
	map_element_iterator_begin(&it);
	do {
		if (map_element_get_type(it.element) == MAP_ELEMENT_TYPE_SURFACE) {
			if (it.element->properties.surface.ownership & (OWNERSHIP_CONSTRUCTION_RIGHTS_OWNED | OWNERSHIP_OWNED)) {
				tiles++;
			}
		}
	} while (map_element_iterator_next(&it));

	if (tiles != RCT2_GLOBAL(RCT2_ADDRESS_PARK_SIZE, sint16)) {
		RCT2_GLOBAL(RCT2_ADDRESS_PARK_SIZE, sint16) = tiles;
		window_invalidate_by_class(WC_PARK_INFORMATION);
	}
	
	return tiles;
}

/**
 * 
 *  rct2: 0x00669EAA
 */
int calculate_park_rating()
{
	int result;

	result = 1150;
	if (RCT2_GLOBAL(RCT2_ADDRESS_PARK_FLAGS, uint32) & 0x4000)
		result = 1050;
	
	// Guests
	{
		rct_peep* peep;
		uint16 spriteIndex;
		int num_happy_peeps;
		short _bp;
		
		// -150 to +3 based on a range of guests from 0 to 2000
		result -= 150 - (min(2000, RCT2_GLOBAL(RCT2_ADDRESS_GUESTS_IN_PARK, uint16)) / 13);

		// Guests, happiness, ?
		num_happy_peeps = 0;
		_bp = 0;
		FOR_ALL_GUESTS(spriteIndex, peep) {
			if (peep->var_2A != 0)
				continue;
			if (peep->happiness > 128)
				num_happy_peeps++;
			if (!(peep->flags & PEEP_FLAGS_LEAVING_PARK))
				continue;
			if (peep->var_C6 <= 89)
				_bp++;
		}
		
		// Peep happiness -500 to +0
		result -= 500;

		if (RCT2_GLOBAL(RCT2_ADDRESS_GUESTS_IN_PARK, uint16) > 0)
			result += 2 * min(250, (num_happy_peeps * 300) / RCT2_GLOBAL(RCT2_ADDRESS_GUESTS_IN_PARK, uint16));

		// ?
		_bp -= 25;
		if (_bp >= 0)
			result -= _bp * 7;
	}

	// Rides
	{
		int i;
		short total_ride_uptime = 0, total_ride_intensity = 0, total_ride_excitement = 0, average_intensity, average_excitement;
		int num_rides, num_exciting_rides = 0;
		rct_ride* ride;

		num_rides = 0;
		FOR_ALL_RIDES(i, ride) {
			total_ride_uptime += 100 - ride->downtime;

			if (ride->excitement != -1){
				total_ride_excitement += ride->excitement / 8;
				total_ride_intensity += ride->intensity / 8;
				num_exciting_rides++;
			}
			num_rides++;
		}
		result -= 200;
		if (num_rides > 0)
			result += (total_ride_uptime / num_rides) * 2;

		result -= 100;

		if (num_exciting_rides>0){
			average_excitement = total_ride_excitement / num_exciting_rides;
			average_intensity = total_ride_intensity / num_exciting_rides;

			average_excitement -= 46;
			if (average_excitement < 0){
				average_excitement = -average_excitement;
			}

			average_intensity -= 65;
			if (average_intensity < 0){
				average_intensity = -average_intensity;
			}

			average_excitement = min(average_excitement / 2, 50);
			average_intensity = min(average_intensity / 2, 50);
			result += 100 - average_excitement - average_intensity;
		}

		total_ride_excitement = min(1000, total_ride_excitement);
		total_ride_intensity = min(1000, total_ride_intensity);
		result -= 200 - ((total_ride_excitement + total_ride_intensity) / 10);
	}

	// Litter
	{
		rct_litter* litter;
		uint16 sprite_idx;
		short num_litter;

		num_litter = 0;
		for (sprite_idx = RCT2_GLOBAL(RCT2_ADDRESS_SPRITES_START_LITTER, uint16); sprite_idx != SPRITE_INDEX_NULL; sprite_idx = litter->next) {
			litter = &(g_sprite_list[sprite_idx].litter);

			// Guessing this eliminates recently dropped litter
			if (litter->var_24 - RCT2_GLOBAL(RCT2_ADDRESS_SCENARIO_TICKS, uint32) >= 7680)
				num_litter++;
		}
		result -= 600 - (4 * (150 - min(150, num_litter)));
	}
	
	result -= RCT2_GLOBAL(0x0135882E, sint16);
	result = clamp(0, result, 999);
	return result;
}

money32 calculate_ride_value(rct_ride *ride)
{
	if (ride->type == RIDE_TYPE_NULL)
		return 0;
	if (ride->value == RIDE_VALUE_UNDEFINED)
		return 0;

	// Fair value * (...)
	return (ride->value * 10) * (
		ride->var_124 + ride->var_126 + ride->var_128 + ride->var_12A +
		ride->var_12C + ride->var_12E + ride->age + ride->running_cost +
		ride->var_134 + ride->var_136 +
		*((uint8*)(0x0097D21E + (ride->type * 8))) * 4
	);
}

/**
 * 
 *  rct2: 0x0066A3F6
 */
money32 calculate_park_value()
{
	int i;
	money32 result;
	rct_ride* ride;

	// Sum ride values
	result = 0;
	for (i = 0; i < 255; i++) {
		ride = &g_ride_list[i];
		result += calculate_ride_value(ride);
	}

	// +7.00 per guest
	result += RCT2_GLOBAL(RCT2_ADDRESS_GUESTS_IN_PARK, uint16) * MONEY(7, 00);

	return result;
}

/**
 * Calculate the company value.
 * Cash + Park Value - Loan
 *
 *  rct2: 0x0066A498
 */
money32 calculate_company_value()
{
	return
		DECRYPT_MONEY(RCT2_GLOBAL(RCT2_ADDRESS_CURRENT_MONEY_ENCRYPTED, sint32)) +
		RCT2_GLOBAL(RCT2_ADDRESS_CURRENT_PARK_VALUE, money32) -
		RCT2_GLOBAL(RCT2_ADDRESS_CURRENT_LOAN, money32);
}

/**
 *
 *  rct2: 0x00667104
 */
void reset_park_entrances()
{
	RCT2_GLOBAL(RCT2_ADDRESS_PARK_NAME, rct_string_id) = 0;

	for (short i = 0; i < 4; i++) {
		RCT2_ADDRESS(RCT2_ADDRESS_PARK_ENTRANCE_X, uint16)[i] = 0x8000;
	}

	RCT2_GLOBAL(RCT2_ADDRESS_PEEP_SPAWNS, uint16) = 0xFFFF;
	RCT2_GLOBAL(0x013573F8, uint16) = 0xFFFF;
}

/**
 * Calculates the probability of a new guest. Also sets total ride value and suggested guest maximum.
 * Total ride value should probably be set else where, as its not just used for guest generation.
 * Suggested guest maximum should probably be an output result, not a global.
 * @returns A probability out of 65535
 *  rct2: 0x0066730A
 */
static int park_calculate_guest_generation_probability()
{
	unsigned int probability;
	int i, suggestedMaxGuests, totalRideValue;
	rct_ride *ride;

	// Calculate suggested guest maximum (based on ride type) and total ride value
	suggestedMaxGuests = 0;
	totalRideValue = 0;
	FOR_ALL_RIDES(i, ride) {
		if (ride->status != RIDE_STATUS_OPEN)
			continue;
		if (ride->lifecycle_flags & RIDE_LIFECYCLE_BROKEN_DOWN)
			continue;
		if (ride->lifecycle_flags & RIDE_LIFECYCLE_CRASHED)
			continue;

		// Add guest score for ride type
		suggestedMaxGuests += RCT2_GLOBAL(0x0097D21E + (ride->type * 8), uint8);

		// Add ride value
		if (ride->value != RIDE_VALUE_UNDEFINED) {
			int rideValue = ride->value - ride->price;
			if (rideValue > 0)
				totalRideValue += rideValue * 2;
		}
	}

	// If difficult guest generation, extra guests are available for good rides
	if (RCT2_GLOBAL(RCT2_ADDRESS_PARK_FLAGS, uint32) & PARK_FLAGS_DIFFICULT_GUEST_GENERATION) {
		suggestedMaxGuests = min(suggestedMaxGuests, 1000);
		FOR_ALL_RIDES(i, ride) {
			if (ride->lifecycle_flags & RIDE_LIFECYCLE_BROKEN_DOWN)
				continue;
			if (ride->lifecycle_flags & RIDE_LIFECYCLE_CRASHED)
				continue;

			if (!ride_type_has_flag(ride->type, RIDE_TYPE_FLAG_HAS_TRACK))
				continue;
			if (!ride_type_has_flag(ride->type, RIDE_TYPE_FLAG_HAS_DATA_LOGGING))
				continue;
			if (!(ride->lifecycle_flags & RIDE_LIFECYCLE_TESTED))
				continue;
			if (ride->length[0] < (600 << 16))
				continue;
			if (ride->excitement < RIDE_RATING(6,00))
				continue;

			// Bonus guests for good ride
			suggestedMaxGuests += RCT2_GLOBAL(0x0097D21E + (ride->type * 8), uint8) * 2;
		}
	}

	suggestedMaxGuests = min(suggestedMaxGuests, 65535);
	RCT2_GLOBAL(RCT2_TOTAL_RIDE_VALUE, uint16) = totalRideValue;
	_suggestedGuestMaximum = suggestedMaxGuests;

	// Begin with 50 + park rating
	probability = 50 + clamp(0, RCT2_GLOBAL(RCT2_ADDRESS_CURRENT_PARK_RATING, uint16) - 200, 650);

	// The more guests, the lower the chance of a new one
	int numGuests = RCT2_GLOBAL(RCT2_ADDRESS_GUESTS_IN_PARK, uint16) + RCT2_GLOBAL(RCT2_ADDRESS_GUESTS_HEADING_FOR_PARK, uint16);
	if (numGuests > suggestedMaxGuests) {
		probability /= 4;

		// Even lower for difficult guest generation
		if (RCT2_GLOBAL(RCT2_ADDRESS_PARK_FLAGS, uint32) & PARK_FLAGS_DIFFICULT_GUEST_GENERATION)
			probability /= 4;
	}

	// Reduces chance for any more than 7000 guests
	if (numGuests > 7000)
		probability /= 4;

	// Check if money is enabled
	if (!(RCT2_GLOBAL(RCT2_ADDRESS_PARK_FLAGS, uint32) & PARK_FLAGS_NO_MONEY)) {
		// Penalty for overpriced entrance fee relative to total ride value
		money16 entranceFee = RCT2_GLOBAL(RCT2_ADDRESS_PARK_ENTRANCE_FEE, money16);
		if (entranceFee > totalRideValue) {
			probability /= 4;

			// Extra penalty for very overpriced entrance fee
			if (entranceFee / 2 > totalRideValue)
				probability /= 4;
		}
	}

	// Reward or penalties for park awards
	for (i = 0; i < MAX_AWARDS; i++) {
		rct_award *award = &RCT2_ADDRESS(RCT2_ADDRESS_AWARD_LIST, rct_award)[i];
		if (award->time == 0)
			continue;

		// +/- 0.25% of the probability
		if (award_is_positive(award->type))
			probability += probability / 4;
		else
			probability -= probability / 4;
	}

	return probability;
}

static void get_random_peep_spawn(rct2_peep_spawn *spawn)
{
	rct2_peep_spawn *peepSpawns = RCT2_ADDRESS(RCT2_ADDRESS_PEEP_SPAWNS, rct2_peep_spawn);

	*spawn = peepSpawns[0];
	if (peepSpawns[1].x != 0xFFFF)
		if (scenario_rand() & 0x80000)
			*spawn = peepSpawns[1];
}

static rct_peep *park_generate_new_guest()
{
	rct_peep *peep = NULL;
	rct2_peep_spawn spawn;
	get_random_peep_spawn(&spawn);

	if (spawn.x != 0xFFFF) {
		spawn.direction ^= 2;
		peep = peep_generate(spawn.x, spawn.y, spawn.z * 16);
		if (peep != NULL) {
			peep->sprite_direction = spawn.direction << 3;
						
			// Get the centre point of the tile the peep is on
			peep->destination_x = (peep->x & 0xFFE0) + 16;
			peep->destination_y = (peep->y & 0xFFE0) + 16;

			peep->destination_tolerence = 5;
			peep->var_76 = 0;
			peep->var_78 = spawn.direction;
			peep->var_37 = 0;
			peep->state = PEEP_STATE_ENTERING_PARK;
		}
	}

	return peep;
}

//This is called via the cheat window.
void generate_new_guest()
{
	park_generate_new_guest();
}

static rct_peep *park_generate_new_guest_due_to_campaign(int campaign)
{
	rct_peep *peep = park_generate_new_guest();
	if (peep != NULL)
		marketing_set_guest_campaign(peep, campaign);
	return peep;
}

static void park_generate_new_guests()
{
	// Generate a new guest for some probability
	if ((int)(scenario_rand() & 0xFFFF) < _guestGenerationProbability) {
		int difficultGeneration = (RCT2_GLOBAL(RCT2_ADDRESS_PARK_FLAGS, uint32) & PARK_FLAGS_DIFFICULT_GUEST_GENERATION) != 0;
		if (!difficultGeneration || _suggestedGuestMaximum + 150 >= RCT2_GLOBAL(RCT2_ADDRESS_GUESTS_IN_PARK, uint16))
			park_generate_new_guest();
	}

	// Extra guests generated by advertising campaigns
	int campaign;
	for (campaign = 0; campaign < ADVERTISING_CAMPAIGN_COUNT; campaign++) {
		if (gMarketingCampaignDaysLeft[campaign] != 0) {
			// Random chance of guest generation
			if ((int)(scenario_rand() & 0xFFFF) < marketing_get_campaign_guest_generation_probability(campaign))
				park_generate_new_guest_due_to_campaign(campaign);
		}
	}
}

/**
 *
 *  rct2: 0x006674F7
 */
void park_update()
{
	if (RCT2_GLOBAL(RCT2_ADDRESS_SCREEN_FLAGS, uint8) & 0x0E)
		return;

	// Every 5 seconds approximately
	if (RCT2_GLOBAL(RCT2_ADDRESS_CURRENT_TICKS, uint8) % 512 == 0) {
		RCT2_GLOBAL(RCT2_ADDRESS_CURRENT_PARK_RATING, uint16) = calculate_park_rating();
		RCT2_GLOBAL(RCT2_ADDRESS_CURRENT_PARK_VALUE, money32) = calculate_park_value();
		RCT2_GLOBAL(RCT2_ADDRESS_CURRENT_COMPANY_VALUE, money32) = calculate_company_value();
		window_invalidate_by_class(WC_FINANCES);
		_guestGenerationProbability = park_calculate_guest_generation_probability();
		RCT2_GLOBAL(RCT2_ADDRESS_BTM_TOOLBAR_DIRTY_FLAGS, uint16) |= BTM_TB_DIRTY_FLAG_PARK_RATING;
		window_invalidate_by_class(WC_PARK_INFORMATION);
	}

	// Generate new guests
	park_generate_new_guests();
}

uint8 calculate_guest_initial_happiness(uint8 percentage) {
	if (percentage < 15) {
		// There is a minimum of 15% happiness
		percentage = 15;
	}
	else if (percentage > 98) {
		// There is a maximum of 98% happiness
		percentage = 98;
	}

	/* The percentages follow this sequence:
		15 17 18 20 21 23 25 26 28 29 31 32 34 36 37 39 40 42 43 45 47 48 50 51 53...

		This sequence can be defined as PI*(9+n)/2 (the value is floored)
		*/
	uint8 n;
	for (n = 1; n < 55; n++) {
		if ((3.14159*(9 + n)) / 2 >= percentage) {
			return (9 + n) * 4;
		}
	}
	return 40; // This is the lowest possible value
}

/**
 *
 *  rct2: 0x0066A231
 */
void park_update_histories()
{
	RCT2_CALLPROC_EBPSAFE(0x0066A231);
}

void park_set_entrance_fee(money32 value)
{
	game_do_command(0, GAME_COMMAND_FLAG_APPLY, 0, 0, GAME_COMMAND_SET_PARK_ENTRANCE_FEE, value, 0);
}

/**
 *
 *  rct2: 0x00669E30
 */
void game_command_set_park_entrance_fee(int *eax, int *ebx, int *ecx, int *edx, int *esi, int *edi, int *ebp)
{
	RCT2_GLOBAL(RCT2_ADDRESS_NEXT_EXPENDITURE_TYPE, uint8) = RCT_EXPENDITURE_TYPE_PARK_ENTRANCE_TICKETS * 4;
	if (*ebx & GAME_COMMAND_FLAG_APPLY) {
		RCT2_GLOBAL(RCT2_ADDRESS_PARK_ENTRANCE_FEE, money16) = (*edi & 0xFFFF);
		window_invalidate_by_class(WC_PARK_INFORMATION);
	}
	*ebx = 0;
}

void park_set_open(int open)
{
	game_do_command(0, GAME_COMMAND_FLAG_APPLY, 0, open << 8, GAME_COMMAND_SET_PARK_OPEN, 0, 0);
}

/**
 *
 *  rct2: 0x00669D4A
 */
void game_command_set_park_open(int* eax, int* ebx, int* ecx, int* edx, int* esi, int* edi, int* ebp)
{
	if (*ebx & GAME_COMMAND_FLAG_APPLY) {
		*ebx = 0;
		return;
	}

	int dh = (*edx >> 8) & 0xFF;

	RCT2_GLOBAL(RCT2_ADDRESS_NEXT_EXPENDITURE_TYPE, uint8) = RCT_EXPENDITURE_TYPE_PARK_ENTRANCE_TICKETS * 4;
	switch (dh) {
	case 0:
		if (RCT2_GLOBAL(RCT2_ADDRESS_PARK_FLAGS, uint32) & PARK_FLAGS_PARK_OPEN) {
			RCT2_GLOBAL(RCT2_ADDRESS_PARK_FLAGS, uint32) &= ~PARK_FLAGS_PARK_OPEN;
			window_invalidate_by_class(WC_PARK_INFORMATION);
		}
		break;
	case 1:
		if (!(RCT2_GLOBAL(RCT2_ADDRESS_PARK_FLAGS, uint32) & PARK_FLAGS_PARK_OPEN)) {
			RCT2_GLOBAL(RCT2_ADDRESS_PARK_FLAGS, uint32) |= PARK_FLAGS_PARK_OPEN;
			window_invalidate_by_class(WC_PARK_INFORMATION);
		}
		break;
	case 2:
		RCT2_GLOBAL(0x01358838, uint32) = *edi;
		window_invalidate_by_class(WC_RIDE);
		break;
	case 3:
		RCT2_GLOBAL(0x0135934C, uint32) = *edi;
		window_invalidate_by_class(WC_RIDE);
		break;
	}

	*ebx = 0;
}

int park_get_entrance_index(int x, int y, int z)
{
	int i;

	for (i = 0; i < 4; i++) {
		if (
			x == RCT2_ADDRESS(RCT2_ADDRESS_PARK_ENTRANCE_X, uint16)[i] &&
			y == RCT2_ADDRESS(RCT2_ADDRESS_PARK_ENTRANCE_Y, uint16)[i] &&
			z == RCT2_ADDRESS(RCT2_ADDRESS_PARK_ENTRANCE_Z, uint16)[i]
		) {
			return i;
		}
	}

	return -1;
}

/**
*
*  rct2: 0x006EC847
*/
void gfx_invalidate_viewport_tile(int x, int y, int z0, int z1)
{
	int x1, y1, x2, y2;
	int tempx;
	x += 16;
	y += 16;
	int rotation = RCT2_GLOBAL(RCT2_ADDRESS_CURRENT_ROTATION, uint32);
	switch (rotation) {
	case 0:
		tempx = x;
		x = -x + y;
		y += tempx;
		break;
	case 1:
		x = -x;
		tempx = x;
		x -= y;
		y += tempx;
		break;
	case 2:
		tempx = x;
		x -= y;
		y = -y - tempx;
		break;
	case 3:
		tempx = x;
		x += y;
		y = -y + tempx;
		break;
	}
	y /= 2;
	x2 = x;
	y2 = y;

	x -= 32;
	y -= 32;
	y -= z0;
	x2 += 32;
	y2 += 32;
	y2 += z1;

	x1 = x;
	y1 = y;
	rct_viewport* viewport = RCT2_GLOBAL(RCT2_ADDRESS_ACTIVE_VIEWPORT_PTR_ARRAY, rct_viewport*);
	while (viewport->width != 0) {
		int viewport_x2 = viewport->view_x + viewport->view_width;
		int viewport_y2 = viewport->view_y + viewport->view_height;
		if (x2 > viewport_x2 && y2 > viewport_y2) {
			if (x1 < viewport->view_x)
				x1 = viewport->view_x;
			if (x2 > viewport_x2)
				x2 = viewport_x2;
			if (y1 < viewport->view_y)
				y1 = viewport->view_y;
			if (y2 > viewport_y2)
				y2 = viewport_y2;

			uint8 zoom = viewport->zoom;
			x1 -= viewport->view_x;
			y1 -= viewport->view_y;
			x2 -= viewport->view_x;
			y2 -= viewport->view_y;
			x1 >>= zoom;
			y1 >>= zoom;
			x2 >>= zoom;
			y2 >>= zoom;
			x1 += viewport->x;
			y1 += viewport->y;
			x2 += viewport->x;
			y2 += viewport->y;
			gfx_set_dirty_blocks(x1, y1, x2, y2);
		}
		viewport++;
	}
}

/**
*
*  rct2: 0x00664D05
*/
void update_park_fences(int x, int y)
{
	if (x <= 0x1FFF && y <= 0x1FFF) {
		int y2;
		int tile_idx, tile_idx2;

		y2 = y;
		tile_idx = (((y & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
		while ((TILE_MAP_ELEMENT_POINTER(tile_idx)->type & 0x3C) != 0) {
			y2 += 8;
			tile_idx = (((y2 & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
		}

		uint8 ownership = TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership;
		uint8 newOwnership = ownership & 0xF0;
		if ((ownership & OWNERSHIP_OWNED) == 0) {
			y2 = y;
			bool breaked = false;
			do {
				tile_idx2 = (((y2 & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
				uint8 type = TILE_MAP_ELEMENT_POINTER(tile_idx2)->type;
				uint8 slope = TILE_MAP_ELEMENT_POINTER(tile_idx2)->properties.surface.slope;
				uint8 flags = TILE_MAP_ELEMENT_POINTER(tile_idx2)->flags;
				if ((type & 0x3C) == 0x10 && (flags & 0x10) == 0 && slope == 0x2) {
					breaked = true;
					break;
				}
				y2 += 8;
			} while ((TILE_MAP_ELEMENT_POINTER(tile_idx2)->flags & 0x80) == 0);
			if (!breaked) {
				x -= 32;
				if (x <= 0x1FFF) {
					y2 = y;
					tile_idx2 = (((y & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
					while ((TILE_MAP_ELEMENT_POINTER(tile_idx2)->type & 0x3C) != 0) {
						y2 += 8;
						tile_idx2 = (((y2 & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
					}
					ownership = TILE_MAP_ELEMENT_POINTER(tile_idx2)->properties.surface.ownership;
					if ((ownership & OWNERSHIP_OWNED) != 0) {
						newOwnership |= 0x8;
					}
				}
				x += 32;
				y -= 32;
				if (y <= 0x1FFF) {
					y2 = y;
					tile_idx2 = (((y & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
					while ((TILE_MAP_ELEMENT_POINTER(tile_idx2)->type & 0x3C) != 0) {
						y2 += 8;
						tile_idx2 = (((y2 & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
					}
					ownership = TILE_MAP_ELEMENT_POINTER(tile_idx2)->properties.surface.ownership;
					if ((ownership & OWNERSHIP_OWNED) != 0) {
						newOwnership |= 0x4;
					}
				}
				x += 32;
				y += 32;
				if (x <= 0x1FFF) {
					y2 = y;
					tile_idx2 = (((y & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
					while ((TILE_MAP_ELEMENT_POINTER(tile_idx2)->type & 0x3C) != 0) {
						y2 += 8;
						tile_idx2 = (((y2 & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
					}
					ownership = TILE_MAP_ELEMENT_POINTER(tile_idx2)->properties.surface.ownership;
					if ((ownership & OWNERSHIP_OWNED) != 0) {
						newOwnership |= 0x2;
					}
				}
				x -= 32;
				y += 32;
				if (y <= 0x1FFF) {
					y2 = y;
					tile_idx2 = (((y & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
					while ((TILE_MAP_ELEMENT_POINTER(tile_idx2)->type & 0x3C) != 0) {
						y2 += 8;
						tile_idx2 = (((y2 & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
					}
					ownership = TILE_MAP_ELEMENT_POINTER(tile_idx2)->properties.surface.ownership;
					if ((ownership & OWNERSHIP_OWNED) != 0) {
						newOwnership |= 0x1;
					}
				}
				y -= 32;
			}
		}
		if (TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership & newOwnership) {
			int z0 = TILE_MAP_ELEMENT_POINTER(tile_idx)->base_height * 8;
			int z1 = z0 + 16;
			gfx_invalidate_viewport_tile(x, y, z0, z1);
		}
		TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership = newOwnership;
	}

}

void park_remove_entrance_segment(int x, int y, int z)
{
	rct_map_element *mapElement;

	mapElement = map_get_first_element_at(x / 32, y / 32);
	do {
		if (map_element_get_type(mapElement) != MAP_ELEMENT_TYPE_ENTRANCE)
			continue;
		if (mapElement->base_height != z)
			continue;
		if (mapElement->properties.entrance.type != ENTRANCE_TYPE_PARK_ENTRANCE)
			continue;

		gfx_invalidate_viewport_tile(x, y, mapElement->base_height * 8, mapElement->clearance_height * 8);
		map_element_remove(mapElement);
		update_park_fences(x, y);
	} while (!map_element_is_last_for_tile(mapElement++));
}

/**
 *
 *  rct2: 0x00666A63
 */
void game_command_remove_park_entrance(int *eax, int *ebx, int *ecx, int *edx, int *esi, int *edi, int *ebp)
{
	int x, y, z, entranceIndex, direction;

	x = *eax & 0xFFFF;
	y = *ecx & 0xFFFF;
	z = *edx * 16;

	RCT2_GLOBAL(RCT2_ADDRESS_NEXT_EXPENDITURE_TYPE, uint32) = RCT_EXPENDITURE_TYPE_LAND_PURCHASE * 4;
	RCT2_GLOBAL(0x009DEA5E, uint16) = x;
	RCT2_GLOBAL(0x009DEA60, uint16) = y;
	RCT2_GLOBAL(0x009DEA62, uint16) = z;

	if (!(*ebx & GAME_COMMAND_FLAG_APPLY)) {
		*ebx = 0;
		return;
	}

	entranceIndex = park_get_entrance_index(x, y, z);
	if (entranceIndex == -1) {
		*ebx = 0;
		return;
	}

	RCT2_ADDRESS(RCT2_ADDRESS_PARK_ENTRANCE_X, uint16)[entranceIndex] = 0x8000;
	direction = (RCT2_ADDRESS(RCT2_ADDRESS_PARK_ENTRANCE_DIRECTION, uint8)[entranceIndex] - 1) & 3;
	z = (*edx & 0xFF) * 2;

	// Centre (sign)
	park_remove_entrance_segment(x, y, z);

	// Left post
	park_remove_entrance_segment(
		x + TileDirectionDelta[direction].x,
		y + TileDirectionDelta[direction].y,
		z
	);

	// Right post
	park_remove_entrance_segment(
		x - TileDirectionDelta[direction].x,
		y - TileDirectionDelta[direction].y,
		z
	);

	*ebx = 0;
}

void park_set_name(const char *name)
{
	RCT2_GLOBAL(RCT2_ADDRESS_GAME_COMMAND_ERROR_TITLE, uint16) = STR_CANT_RENAME_PARK;
	game_do_command(1, GAME_COMMAND_FLAG_APPLY, 0, *((int*)(name + 0)), GAME_COMMAND_SET_PARK_NAME, *((int*)(name + 8)), *((int*)(name + 4)));
	game_do_command(2, GAME_COMMAND_FLAG_APPLY, 0, *((int*)(name + 12)), GAME_COMMAND_SET_PARK_NAME, *((int*)(name + 20)), *((int*)(name + 16)));
	game_do_command(0, GAME_COMMAND_FLAG_APPLY, 0, *((int*)(name + 24)), GAME_COMMAND_SET_PARK_NAME, *((int*)(name + 32)), *((int*)(name + 28)));
}

/**
 *
 *  rct2: 0x00669C6D
 */
void game_command_set_park_name(int *eax, int *ebx, int *ecx, int *edx, int *esi, int *edi, int *ebp)
{
	rct_string_id newUserStringId;
	char oldName[128];
	static char newName[128];

	int nameChunkIndex = *eax & 0xFFFF;

	RCT2_GLOBAL(RCT2_ADDRESS_NEXT_EXPENDITURE_TYPE, uint8) = RCT_EXPENDITURE_TYPE_LANDSCAPING * 4;
	if (*ebx & GAME_COMMAND_FLAG_APPLY) {
		int nameChunkOffset = nameChunkIndex - 1;
		if (nameChunkOffset < 0)
			nameChunkOffset = 2;
		nameChunkOffset *= 12;
		RCT2_GLOBAL(newName + nameChunkOffset + 0, uint32) = *edx;
		RCT2_GLOBAL(newName + nameChunkOffset + 4, uint32) = *ebp;
		RCT2_GLOBAL(newName + nameChunkOffset + 8, uint32) = *edi;
	}

	if (nameChunkIndex != 0) {
		*ebx = 0;
		return;
	}

	format_string(oldName, RCT2_GLOBAL(RCT2_ADDRESS_PARK_NAME, rct_string_id), &RCT2_GLOBAL(RCT2_ADDRESS_PARK_NAME_ARGS, uint32));
	if (strcmp(oldName, newName) == 0) {
		*ebx = 0;
		return;
	}

	if (newName[0] == 0) {
		RCT2_GLOBAL(RCT2_ADDRESS_GAME_COMMAND_ERROR_TEXT, uint16) = STR_INVALID_RIDE_ATTRACTION_NAME;
		*ebx = MONEY32_UNDEFINED;
		return;
	}

	newUserStringId = user_string_allocate(4, newName);
	if (newUserStringId == 0) {
		RCT2_GLOBAL(RCT2_ADDRESS_GAME_COMMAND_ERROR_TEXT, uint16) = STR_INVALID_NAME_FOR_PARK;
		*ebx = MONEY32_UNDEFINED;
		return;
	}

	if (*ebx & GAME_COMMAND_FLAG_APPLY) {
		// Free the old ride name
		user_string_free(RCT2_GLOBAL(RCT2_ADDRESS_PARK_NAME, rct_string_id));
		RCT2_GLOBAL(RCT2_ADDRESS_PARK_NAME, rct_string_id) = newUserStringId;

		gfx_invalidate_screen();
	} else {
		user_string_free(newUserStringId);
	}

	*ebx = 0;
}

int map_buy_land_rights_for_tile(int x, int y, int setting, int flags) {
	int y2;
	int cost;
	int tile_idx;

	y2 = y;
	cost = 0;
	tile_idx = (((y & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
	while ((TILE_MAP_ELEMENT_POINTER(tile_idx)->type & 0x3C) != 0) {
		y2 += 8;
		tile_idx = (((y2 & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
	}
	uint8 ownership = TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership;
	switch (setting) {
	case 0:
		if ((ownership & OWNERSHIP_OWNED) != 0) { // If the land is already owned
			cost = 0;
			return cost;
		}
		else if ((RCT2_GLOBAL(RCT2_ADDRESS_SCREEN_FLAGS, uint8) & SCREEN_FLAGS_SCENARIO_EDITOR) != 0 || (ownership & OWNERSHIP_AVAILABLE) == 0) {
			RCT2_GLOBAL(RCT2_ADDRESS_GAME_COMMAND_ERROR_TEXT, uint16) = 1726; // Land not for sale!
			cost = 0;// MONEY32_UNDEFINED;
			return cost;
		}
		else {
			if ((flags & 1) != 0) {
				TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership |= OWNERSHIP_OWNED;
				update_park_fences(x, y);
				update_park_fences(x - 32, y);
				update_park_fences(x + 32, y);
				update_park_fences(x, y + 32);
				update_park_fences(x, y - 32);
			}
			cost = RCT2_GLOBAL(RCT2_ADDRESS_LAND_COST, uint16);
			return cost;
		}
		break;
	case 1:
		if ((flags & 1) != 0) {
			TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership &= 0xCF;
			update_park_fences(x, y);
			update_park_fences(x - 32, y);
			update_park_fences(x + 32, y);
			update_park_fences(x, y + 32);
			update_park_fences(x, y - 32);
		}
		cost = 0;
		break;
	case 2:
		if ((ownership & (OWNERSHIP_OWNED | OWNERSHIP_CONSTRUCTION_RIGHTS_OWNED)) != 0) { // If the land or construction rights are already owned
			cost = 0;
			return cost;
		}
		else if ((RCT2_GLOBAL(RCT2_ADDRESS_SCREEN_FLAGS, uint8) & SCREEN_FLAGS_SCENARIO_EDITOR) != 0 || (ownership & OWNERSHIP_CONSTRUCTION_RIGHTS_AVAILABLE) == 0) {
			RCT2_GLOBAL(RCT2_ADDRESS_GAME_COMMAND_ERROR_TEXT, uint16) = 1727; // Construction rights not for sale!
			cost = 0;// MONEY32_UNDEFINED;
			return cost;
		}
		else {
			if ((flags & 1) != 0) {
				TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership |= OWNERSHIP_CONSTRUCTION_RIGHTS_OWNED;
				uint16 baseHeight = TILE_MAP_ELEMENT_POINTER(tile_idx)->base_height;
				baseHeight *= 8;
				gfx_invalidate_viewport_tile(x, y, baseHeight, baseHeight + 16);
			}
			cost = RCT2_GLOBAL(RCT2_ADDRESS_CONSTRUCTION_RIGHTS_COST, uint16);
			return cost;
		}
		break;
	case 3:
		if ((flags & 1) != 0) {
			TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership &= 0xEF;
			uint16 baseHeight = TILE_MAP_ELEMENT_POINTER(tile_idx)->base_height;
			baseHeight *= 8;
			gfx_invalidate_viewport_tile(x, y, baseHeight, baseHeight + 16);
		}
		cost = 0;
		break;
		break;
	case 4:
		if ((flags & 1) != 0) {
			TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership |= OWNERSHIP_AVAILABLE;
			uint16 baseHeight = TILE_MAP_ELEMENT_POINTER(tile_idx)->base_height;
			baseHeight *= 8;
			gfx_invalidate_viewport_tile(x, y, baseHeight, baseHeight + 16);
		}
		cost = 0;
		break;
	case 5:
		if ((flags & 1) != 0) {
			TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership |= OWNERSHIP_CONSTRUCTION_RIGHTS_AVAILABLE;
			uint16 baseHeight = TILE_MAP_ELEMENT_POINTER(tile_idx)->base_height;
			baseHeight *= 8;
			gfx_invalidate_viewport_tile(x, y, baseHeight, baseHeight + 16);
		}
		cost = 0;
		break;
	default:
		if (x <= 32) {
			RCT2_GLOBAL(RCT2_ADDRESS_GAME_COMMAND_ERROR_TEXT, uint16) = 3215;
			cost = 0;// MONEY32_UNDEFINED;
			return cost;
		}
		else if (y <= 32) {
			int ebp = RCT2_GLOBAL(RCT2_ADDRESS_MAP_SIZE_UNITS, uint16);
			ebp -= 32;
			if (x >= ebp || y >= ebp) {
				RCT2_GLOBAL(RCT2_ADDRESS_GAME_COMMAND_ERROR_TEXT, uint16) = 3215;
				cost = 0;// MONEY32_UNDEFINED;
				return cost;
			}
			else {
				int tile_idx2 = (((y & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
				ownership = TILE_MAP_ELEMENT_POINTER(tile_idx2)->properties.surface.ownership;
				y2 = y;
				do {
					y2 += 8;
					tile_idx2 = (((y2 & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
					if ((TILE_MAP_ELEMENT_POINTER(tile_idx2)->type & 0x3C) == 0x10) {
						cost = 0;
						return cost;
					}

				} while ((TILE_MAP_ELEMENT_POINTER(((((y - 8) & 0xFFE0) * 256) + (x & 0xFFE0)) / 32)->flags & 0x80) == 0);

				uint8 bh = (flags & 0xFF00) >> 4;
				if (bh == (TILE_MAP_ELEMENT_POINTER(tile_idx2)->properties.surface.ownership & 0xF0)) {
					cost = 0;
					return cost;
				}
				else {
					if ((cost & 1) == 0) {
						cost = RCT2_GLOBAL(RCT2_ADDRESS_LAND_COST, uint16);
						return cost;
					}
					if ((bh & 0xF0) == 0) {
						uint16 bp = RCT2_GLOBAL(RCT2_ADDRESS_PEEP_SPAWNS, uint16);
						if (x != (bp & 0xFFE0)) {
							bp = RCT2_GLOBAL(RCT2_ADDRESS_PEEP_SPAWNS + 2, uint16);
							if (y != (bp & 0xFFE0)) {
								RCT2_GLOBAL(RCT2_ADDRESS_PEEP_SPAWNS, uint16) = 0xFFFF;
							}
						}
						bp = RCT2_GLOBAL(0x13573F8, uint16);
						if (x != (bp & 0xFFE0)) {
							bp = RCT2_GLOBAL(0x13573F8 + 2, uint16);
							if (y != (bp & 0xFFE0)) {
								RCT2_GLOBAL(0x13573F8, uint16) = 0xFFFF;
							}
						}
					}
					TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership &= 0x0F;
					TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership |= bh;
					update_park_fences(x, y);
					update_park_fences(x - 32, y);
					update_park_fences(x + 32, y);
					update_park_fences(x, y + 32);
					update_park_fences(x, y - 32);
					RCT2_GLOBAL(0x9E2E28, uint8) |= 1;

					cost = 0;
					return cost;
				}
			}
		}
		break;
	}
	return cost;
}

int map_buy_land_rights(int x0, int y0, int x1, int y1, int setting, int flags)
{
	int x, y, z;
	money32 totalCost, cost;
	RCT2_GLOBAL(RCT2_ADDRESS_NEXT_EXPENDITURE_TYPE, uint8) = RCT_EXPENDITURE_TYPE_LAND_PURCHASE * 4;

	if (x1 == 0 && y1 == 0) {
		x1 = x0;
		y1 = y0;
	}

	x = (x0 + x1) / 2 + 16;
	y = (y0 + y1) / 2 + 16;
	z = map_element_height(x, y);
	RCT2_GLOBAL(0x009DEA5E, uint16) = x;
	RCT2_GLOBAL(0x009DEA60, uint16) = y;
	RCT2_GLOBAL(0x009DEA62, uint16) = z;

	// Game command modified to accept selection size
	totalCost = 0;
	RCT2_GLOBAL(RCT2_ADDRESS_GAME_COMMAND_ERROR_TEXT, uint16) = STR_CONSTRUCTION_NOT_POSSIBLE_WHILE_GAME_IS_PAUSED;
	if ((RCT2_GLOBAL(RCT2_ADDRESS_SCREEN_FLAGS, uint8) & SCREEN_FLAGS_SCENARIO_EDITOR) != 0 || RCT2_GLOBAL(RCT2_ADDRESS_GAME_PAUSED, uint8) == 0) {
		for (y = y0; y <= y1; y += 32) {
			for (x = x0; x <= x1; x += 32) {
				cost = map_buy_land_rights_for_tile(x, y, setting, flags);
				if (cost == MONEY32_UNDEFINED)
					return MONEY32_UNDEFINED;

				totalCost += cost;
			}
		}
	}

	return totalCost;
}

/**
*
*  rct2: 0x006649BD
*/
void game_command_buy_land_rights(int *eax, int *ebx, int *ecx, int *edx, int *esi, int *edi, int *ebp)
{
	*ebx = map_buy_land_rights(
		(*eax & 0xFFFF),
		(*ecx & 0xFFFF),
		(*edi & 0xFFFF),
		(*ebp & 0xFFFF),
		(*edx & 0xFF00) >> 8,
		*ebx & 0xFF
	);
	// Original decompiled function for Duncan to look at:

	/*RCT2_GLOBAL(RCT2_ADDRESS_NEXT_EXPENDITURE_TYPE, uint8) = RCT_EXPENDITURE_TYPE_LAND_PURCHASE * 4;
	int x = *eax;
	int y = *ecx;
	int y2 = *ecx;
	int z = map_element_height(x + 16, y + 16);

	RCT2_GLOBAL(0x009DEA5E, uint16) = x + 16;
	RCT2_GLOBAL(0x009DEA60, uint16) = y + 16;
	RCT2_GLOBAL(0x009DEA62, uint16) = z;

	RCT2_GLOBAL(RCT2_ADDRESS_GAME_COMMAND_ERROR_TEXT, uint16) = STR_CONSTRUCTION_NOT_POSSIBLE_WHILE_GAME_IS_PAUSED;
	if ((RCT2_GLOBAL(RCT2_ADDRESS_SCREEN_FLAGS, uint8) & SCREEN_FLAGS_SCENARIO_EDITOR) != 0 || RCT2_GLOBAL(RCT2_ADDRESS_GAME_PAUSED, uint8) == 0) {
		
		int tile_idx = (((y & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
		while ((TILE_MAP_ELEMENT_POINTER(tile_idx)->type & 0x3C) != 0) {
			y += 8;
			tile_idx = (((y & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
		}
		uint8 ownership = TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership;
		uint8 flags = (*edx & 0xFF00) >> 8;
		switch (flags) {
		case 0:
			if ((ownership & OWNERSHIP_OWNED) != 0) { // If the land is already owned
				*ebx = 0;
				return;
			} 
			else if ((RCT2_GLOBAL(RCT2_ADDRESS_SCREEN_FLAGS, uint8) & SCREEN_FLAGS_SCENARIO_EDITOR) != 0 || (ownership & OWNERSHIP_AVAILABLE) == 0) {
				RCT2_GLOBAL(RCT2_ADDRESS_GAME_COMMAND_ERROR_TEXT, uint16) = 1726; // Land not for sale!
				*ebx = 0x80000000;
				return;
			}
			else {
				if ((*ebx & 1) != 0) {
					TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership |= OWNERSHIP_OWNED;
					update_park_fences(x, y2);
					update_park_fences(x - 32, y2);
					update_park_fences(x + 32, y2);
					update_park_fences(x, y2 + 32);
					update_park_fences(x, y2 - 32);
				}
				*ebx = RCT2_GLOBAL(RCT2_ADDRESS_LAND_COST, uint16);
				return;
			}
			break;
		case 1:
			if ((*ebx & 1) != 0) {
				TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership &= 0xCF;
				update_park_fences(x, y2);
				update_park_fences(x - 32, y2);
				update_park_fences(x + 32, y2);
				update_park_fences(x, y2 + 32);
				update_park_fences(x, y2 - 32);
			}
			*ebx = 0;
			break;
		case 2:
			if ((ownership & (OWNERSHIP_OWNED | OWNERSHIP_CONSTRUCTION_RIGHTS_OWNED)) != 0) { // If the land or construction rights are already owned
				*ebx = 0;
				return;
			}
			else if ((RCT2_GLOBAL(RCT2_ADDRESS_SCREEN_FLAGS, uint8) & SCREEN_FLAGS_SCENARIO_EDITOR) != 0 || (ownership & OWNERSHIP_CONSTRUCTION_RIGHTS_AVAILABLE) == 0) {
				RCT2_GLOBAL(RCT2_ADDRESS_GAME_COMMAND_ERROR_TEXT, uint16) = 1727; // Construction rights not for sale!
				*ebx = 0x80000000;
				return;
			}
			else {
				if ((*ebx & 1) != 0) {
					TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership |= OWNERSHIP_CONSTRUCTION_RIGHTS_OWNED;
					uint16 baseHeight = TILE_MAP_ELEMENT_POINTER(tile_idx)->base_height;
					baseHeight *= 8;
					gfx_invalidate_viewport_tile(x, y2, baseHeight, baseHeight + 16);
				}
				*ebx = RCT2_GLOBAL(RCT2_ADDRESS_CONSTRUCTION_RIGHTS_COST, uint16);
				return;
			}
			break;
		case 3:
			if ((*ebx & 1) != 0) {
				TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership &= 0xEF;
				uint16 baseHeight = TILE_MAP_ELEMENT_POINTER(tile_idx)->base_height;
				baseHeight *= 8;
				gfx_invalidate_viewport_tile(x, y2, baseHeight, baseHeight + 16);
			}
			*ebx = 0;
			break;
			break;
		case 4:
			if ((*ebx & 1) != 0) {
				TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership |= OWNERSHIP_AVAILABLE;
				uint16 baseHeight = TILE_MAP_ELEMENT_POINTER(tile_idx)->base_height;
				baseHeight *= 8;
				gfx_invalidate_viewport_tile(x, y2, baseHeight, baseHeight + 16);
			}
			*ebx = 0;
			break;
		case 5:
			if ((*ebx & 1) != 0) {
				TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership |= OWNERSHIP_CONSTRUCTION_RIGHTS_AVAILABLE;
				uint16 baseHeight = TILE_MAP_ELEMENT_POINTER(tile_idx)->base_height;
				baseHeight *= 8;
				gfx_invalidate_viewport_tile(x, y2, baseHeight, baseHeight + 16);
			}
			*ebx = 0;
			break;
		default:
			if (x <= 32) {
				RCT2_GLOBAL(RCT2_ADDRESS_GAME_COMMAND_ERROR_TEXT, uint16) = 3215;
				*ebx = 0x80000000;
				return;
			}
			else if (y <= 32) {
				*ebp = RCT2_GLOBAL(RCT2_ADDRESS_MAP_SIZE_UNITS, uint16);
				*ebp -= 32;
				if (x >= *ebp || y >= *ebp) {
					RCT2_GLOBAL(RCT2_ADDRESS_GAME_COMMAND_ERROR_TEXT, uint16) = 3215;
					*ebx = 0x80000000;
					return;
				}
				else {
					int tile_idx2 = (((y & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
					ownership = TILE_MAP_ELEMENT_POINTER(tile_idx2)->properties.surface.ownership;
					do {
						y += 8;
						tile_idx2 = (((y & 0xFFE0) * 256) + (x & 0xFFE0)) / 32;
						if ((TILE_MAP_ELEMENT_POINTER(tile_idx2)->type & 0x3C) == 0x10) {
							*ebx = 0;
							return;
						}
						
					} while ((TILE_MAP_ELEMENT_POINTER(((((y - 8) & 0xFFE0) * 256) + (x & 0xFFE0)) / 32)->flags & 0x80) == 0);

					uint8 bh = (*ebx & 0xFF00) >> 4;
					if (bh == (TILE_MAP_ELEMENT_POINTER(tile_idx2)->properties.surface.ownership & 0xF0)) {
						*ebx = 0;
						return;
					}
					else {
						if ((*ebx & 1) == 0) {
							*ebx = RCT2_GLOBAL(RCT2_ADDRESS_LAND_COST, uint16);
							return;
						}
						if ((bh & 0xF0) == 0) {
							uint16 bp = RCT2_GLOBAL(RCT2_ADDRESS_PEEP_SPAWNS, uint16);
							if (x != (bp & 0xFFE0)) {
								bp = RCT2_GLOBAL(RCT2_ADDRESS_PEEP_SPAWNS + 2, uint16);
								if (y2 != (bp & 0xFFE0)) {
									RCT2_GLOBAL(RCT2_ADDRESS_PEEP_SPAWNS, uint16) = 0xFFFF;
								}
							}
							bp = RCT2_GLOBAL(0x13573F8, uint16);
							if (x != (bp & 0xFFE0)) {
								bp = RCT2_GLOBAL(0x13573F8 + 2, uint16);
								if (y2 != (bp & 0xFFE0)) {
									RCT2_GLOBAL(0x13573F8, uint16) = 0xFFFF;
								}
							}
						}
						TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership &= 0x0F;
						TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership |= bh;
						update_park_fences(x, y2);
						update_park_fences(x - 32, y2);
						update_park_fences(x + 32, y2);
						update_park_fences(x, y2 + 32);
						update_park_fences(x, y2 - 32);
						RCT2_GLOBAL(0x9E2E28, uint8) |= 1;

						*ebx = 0;
						return;
					}
				}
			}
			break;
		}
	}
	else {
		// Should this ever be called? esi is never set properly
		if ((*ebx & 1) != 0) {
			//TILE_MAP_ELEMENT_POINTER(tile_idx)->properties.surface.ownership |= OWNERSHIP_OWNED;
			update_park_fences(x, y);
			update_park_fences(x - 32, y2);
			update_park_fences(x + 32, y2);
			update_park_fences(x, y2 + 32);
			update_park_fences(x, y2 - 32);
		}
		*ebx = RCT2_GLOBAL(RCT2_ADDRESS_LAND_COST, uint16);
	}*/
}
