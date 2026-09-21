#include "StatsOverlay.h"

#include <algorithm>
#include <cstdio>

using namespace std;

static const Color White = 0xFFFFFFFF, Grey = 0x808080FF,
		   MaxColor = 0x40FF40FF, MeanColor = 0xFFD040FF;

static string fmt (const char *f, double a, double b = 0, double c = 0) {
    char buf[128];
    snprintf(buf, sizeof(buf), f, a, b, c);
    return buf;
}

static void drawChart (Display &d, const vector<GenerationStats> &h,
		       float x, float y, float w, float ht)
{
    //Frame
    d.line(x, y, x+w, y, Grey);
    d.line(x+w, y, x+w, y+ht, Grey);
    d.line(x+w, y+ht, x, y+ht, Grey);
    d.line(x, y+ht, x, y, Grey);

    double lo = h[0].meanFitness, hi = h[0].maxFitness;
    for (const GenerationStats &s : h) {
	lo = min(lo, s.meanFitness);
	hi = max(hi, s.maxFitness);
    }
    if (hi - lo < 1e-9) hi = lo + 1;

    size_t n = h.size();
    auto px = [&](size_t i) {
	return n > 1 ? x + w * i / (float)(n-1) : x + w/2;
    };
    auto py = [&](double v) {
	return y + ht - (float)((v - lo) / (hi - lo)) * ht;
    };

    for (size_t i = 1; i < n; ++i) {
	d.line(px(i-1), py(h[i-1].meanFitness), px(i), py(h[i].meanFitness),
	       MeanColor);
	d.line(px(i-1), py(h[i-1].maxFitness), px(i), py(h[i].maxFitness),
	       MaxColor);
    }

    d.text(x + 3, y + 2, fmt("%.3g", hi), Grey);
    d.text(x + 3, y + ht - 14, fmt("%.3g", lo), Grey);
    d.text(x + w - 60, y + ht + 3, "max", MaxColor);
    d.text(x + w - 30, y + ht + 3, "mean", MeanColor);
}

void drawStatsOverlay (Display &d, const string &title,
		       const vector<GenerationStats> &history)
{
    const float lineH = 14;
    float y = 8;
    d.text(10, y, title);
    if (history.empty()) return;

    const GenerationStats &s = history.back();
    y += lineH;
    d.text(10, y, fmt("max %.4f", s.maxFitness), MaxColor);
    d.text(110, y, fmt("mean %.4f", s.meanFitness), MeanColor);
    y += lineH;
    d.text(10, y, fmt("pop %.0f  species %.0f  diversity %.2f",
		      s.populationSize, s.nSpecies, s.meanCompat));
    y += lineH;
    d.text(10, y, fmt("best net: %.0f hidden, %.0f links",
		      s.bestHiddenNodes, s.bestEnabledLinks));

    const float w = 200, h = 80;
    drawChart(d, history, d.width() - w - 10, 10, w, h);
}
