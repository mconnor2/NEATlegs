#ifndef __STATS_OVERLAY_H
#define __STATS_OVERLAY_H

#include <string>
#include <vector>

#include "Display.h"
#include "Stats.h"

/**
 * Draw run statistics over a simulation view: a title line, the latest
 * generation's numbers, and a max/mean fitness history chart in the top
 * right corner.  history may be empty (e.g. when replaying a saved genome),
 * in which case only the title is drawn.
 */
void drawStatsOverlay (Display &d, const std::string &title,
		       const std::vector<GenerationStats> &history);

#endif
