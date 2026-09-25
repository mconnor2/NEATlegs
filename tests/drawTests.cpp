// Drawing without a window: what the physics draws (Renderer), how
// BoxScreen maps it onto pixels (Canvas), and the stats overlay.
#include <cmath>
#include <string>
#include <vector>

#include "check.h"
#include "testCreature.h"

#include "BoxScreen.h"
#include "Creature.h"
#include "Renderer.h"
#include "StatsOverlay.h"
#include "World.h"

using namespace std;

namespace {

struct RecordingRenderer : Renderer {
    struct Poly { vector<Vec2> points; Color c; };
    struct Circ { Vec2 centre; float radius; Color c; };
    struct Seg { Vec2 a, b; Color c; };
    vector<Poly> polygons;
    vector<Circ> circles;
    vector<Seg> segments;

    void polygon (const Vec2 *p, int n, Color c) override {
        polygons.push_back({vector<Vec2>(p, p + n), c});
    }
    void circle (Vec2 centre, float r, Color c) override {
        circles.push_back({centre, r, c});
    }
    void segment (Vec2 a, Vec2 b, Color c) override {
        segments.push_back({a, b, c});
    }
};

struct RecordingCanvas : Canvas {
    struct Line { float x1, y1, x2, y2; Color c; };
    struct Circ { float x, y, r; Color c; };
    struct Pt { float x, y; };
    struct Text { float x, y; string s; };
    int w, h;
    vector<Line> lines;
    vector<Circ> circles;
    vector<Pt> points;
    vector<Text> texts;

    RecordingCanvas (int _w = 640, int _h = 480) : w(_w), h(_h) { }
    int width () const override { return w; }
    int height () const override { return h; }
    void line (float x1, float y1, float x2, float y2, Color c) override {
        lines.push_back({x1, y1, x2, y2, c});
    }
    void circle (float x, float y, float r, Color c) override {
        circles.push_back({x, y, r, c});
    }
    void point (float x, float y, Color) override { points.push_back({x, y}); }
    void text (float x, float y, const string &s, Color) override {
        texts.push_back({x, y, s});
    }
    bool hasText (const string &part) const {
        for (const Text &t : texts)
            if (t.s.find(part) != string::npos) return true;
        return false;
    }
};

bool near (Vec2 a, Vec2 b, double tol = 1e-5) {
    return fabs(a.x - b.x) <= tol && fabs(a.y - b.y) <= tol;
}

}

// Ground, then per limb its boxes (polygons) and balls (circles), then
// muscles (segments)
TEST(worldDrawsEveryShape) {
    CreatureSpec spec = testCreature::spec();
    World w;
    CreatureP c = w.createCreature(spec);
    RecordingRenderer r;
    w.draw(r);

    CHECK(r.polygons.size() == 1u + 2u);        // ground + two limb boxes
    CHECK(r.circles.size() == 1u);
    CHECK(r.segments.size() == 1u);
    for (auto &p : r.polygons) CHECK(p.points.size() == 4u && p.c == BodyColor);
    CHECK(r.circles[0].c == BallColor && r.segments[0].c == MuscleColor);

    // Ground: 200 x 20 m box whose top is y = 0
    float top = -1e9, left = 1e9;
    for (Vec2 p : r.polygons[0].points) {
        top = max(top, p.y);
        left = min(left, p.x);
    }
    CHECK_NEAR(top, 0.0, 1e-5);
    CHECK_NEAR(left, -100.0, 1e-4);

    // Limb "a" is unrotated: its box corners are position +- (w, h)
    const LimbSpec &a = spec.limbs[0];
    const ShapeSpec &box = a.shapes[0];
    Vec2 corner = {a.position.x + box.w, a.position.y + box.h};
    bool found = false;
    for (Vec2 p : r.polygons[1].points) found = found || near(p, corner);
    CHECK(found);

    // The ball sits at its limb's local offset, in world coordinates
    BodyId b = c->bodies()[1];
    CHECK(near(r.circles[0].centre,
               b2Body_GetWorldPoint(b, spec.limbs[1].shapes[1].position)));
    CHECK(r.circles[0].radius == spec.limbs[1].shapes[1].radius);

    // Muscle ends are its attachment points
    CHECK(near(r.segments[0].a, b2Body_GetWorldPoint(c->bodies()[0],
                                                      spec.muscles[0].pos1)));
}

// 100 px/m on a 640x480 canvas: world origin at the bottom centre, y up
TEST(boxScreenMapsWorldToPixels) {
    RecordingCanvas canvas;
    BoxScreen s(&canvas, 100.0f);
    s.segment({0, 0}, {1, 1}, 0x123456FF);
    CHECK(canvas.lines.size() == 1u);
    auto &l = canvas.lines[0];
    CHECK(l.x1 == 320 && l.y1 == 479 && l.x2 == 420 && l.y2 == 379);
    CHECK(l.c == 0x123456FFu);

    s.circle({0, 1}, 0.5f, BallColor);
    CHECK(canvas.circles.size() == 1u);
    CHECK(canvas.circles[0].x == 320 && canvas.circles[0].y == 379);
    CHECK(canvas.circles[0].r == 50);

    // A polygon is drawn as a closed loop
    Vec2 sq[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    canvas.lines.clear();
    s.polygon(sq, 4, BodyColor);
    CHECK(canvas.lines.size() == 4u);
    CHECK(canvas.lines[3].x2 == canvas.lines[0].x1 &&
          canvas.lines[3].y2 == canvas.lines[0].y1);
}

TEST(boxScreenCameraFollows) {
    RecordingCanvas canvas;
    BoxScreen s(&canvas, 100.0f);
    Vec2 p;

    s.keepViewable({10, 0});                    // far right
    s.box2pixel({10, 0}, p);
    CHECK_NEAR(p.x, canvas.width() - BoxScreen::SideBorder, 1e-3);

    s.keepViewable({10, 20});                   // far above
    s.box2pixel({10, 20}, p);
    CHECK_NEAR(p.y, BoxScreen::TopBottomBorder, 1e-3);

    Vec2 before;
    s.box2pixel({10, 20}, before);
    s.keepViewable({10, 20});                   // already in view: no change
    s.box2pixel({10, 20}, p);
    CHECK(p.x == before.x && p.y == before.y);
}

TEST(boxScreenGridCoversCanvas) {
    RecordingCanvas canvas;
    BoxScreen s(&canvas, 100.0f);
    s.drawGrid();
    // One 5-point cross per metre: 7 columns x 5 rows at 100 px/m
    CHECK(canvas.points.size() == 5u * 7u * 5u);
    for (auto &p : canvas.points)
        CHECK(p.x >= -1 && p.x <= canvas.width() &&
              p.y >= -1 && p.y <= canvas.height());
}

TEST(boxScreenWithoutCanvasIsSafe) {
    BoxScreen s(nullptr, 100.0f);
    Vec2 sq[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    s.polygon(sq, 4, BodyColor);
    s.circle({0, 0}, 1, BallColor);
    s.segment({0, 0}, {1, 1}, MuscleColor);
    s.drawGrid();
    s.keepViewable({100, 100});
    CHECK(true);
}

// The whole scene through BoxScreen: every physics primitive becomes pixels
TEST(sceneDrawsThroughBoxScreen) {
    CreatureSpec spec = testCreature::spec();
    World w;
    w.createCreature(spec);
    RecordingCanvas canvas;
    BoxScreen s(&canvas, 100.0f);
    w.draw(s);
    CHECK(canvas.lines.size() == 3u * 4u + 1u); // 3 boxes + 1 muscle
    CHECK(canvas.circles.size() == 1u);
}

TEST(statsOverlayTitleOnly) {
    RecordingCanvas canvas;
    drawStatsOverlay(canvas, "Replay x.genome", {});
    CHECK(canvas.texts.size() == 1u && canvas.texts[0].s == "Replay x.genome");
    CHECK(canvas.lines.empty());
}

TEST(statsOverlayWithHistory) {
    vector<GenerationStats> h(3);
    for (int i = 0; i < 3; ++i) {
        h[i].generation = i;
        h[i].maxFitness = 1.0 + i;
        h[i].meanFitness = 0.5 + i * 0.25;
        h[i].populationSize = 200;
        h[i].nSpecies = 12;
        h[i].meanCompat = 3.5;
    }
    RecordingCanvas canvas;
    drawStatsOverlay(canvas, "Gen 2", h);
    CHECK(canvas.hasText("Gen 2"));
    CHECK(canvas.hasText("max 3.0000"));
    CHECK(canvas.hasText("mean 1.0000"));
    CHECK(canvas.hasText("pop 200  species 12  diversity 3.50"));
    // Chart: 4 frame lines, then max and mean lines between the 3 points
    CHECK(canvas.lines.size() == 4u + 2u * 2u);
    for (auto &l : canvas.lines)
        CHECK(l.x1 >= canvas.width() - 211 && l.x2 <= canvas.width());
}

int main () {
    return check::runTests();
}
