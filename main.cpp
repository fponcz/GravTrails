// =============================================================================
// GravTrails Engine
// Interactive orbital gravity simulation app written in C++ using SDL2
// Demonstrates gravitational motion and orbital trails
// =============================================================================

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace std;


// Math helpers ====================================================================

// Clamp value to a closed range
static double clampDouble(double value, double minValue, double maxValue) {
    return max(minValue, min(maxValue, value));
}

static int clampInt(int value, int minValue, int maxValue) {
    return max(minValue, min(maxValue, value));
}

// Linear interpolation
static double lerpDouble(double a, double b, double t) {
    return a + (b - a) * t;
}

static int lerpInt(int a, int b, double t) {
    return (int)round(lerpDouble((double)a, (double)b, t));
}



// Structures =========================================================================

// 2D vector
struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

struct Point {
    double x = 0.0;
    double y = 0.0;
};

// Struct for creating planets and stars
struct Body {
    Vec2 pos;
    Vec2 vel;
    double mass = 1.0;
    int radius = 5;
    int colorR = 255;
    int colorG = 255;
    int colorB = 255;
    bool fixed = false;
};

enum class SimulationMode {
    SINGLE_STAR,
    DOUBLE_STAR
};

struct BackgroundStar {
    float x = 0.0f;
    float y = 0.0f;
    float size = 1.0f;
    float phase = 0.0f;
    Uint8 alpha = 255;
};

struct Slider {
    SDL_Rect trackRect{};
    double minValue = 0.0;
    double maxValue = 1.0;
    double* boundValue = nullptr;
    string label;
    bool dragging = false;
};

struct AppState {
    const int windowWidth = 1200;
    const int windowHeight = 760;
    const int panelHeight = 108;

    double singleStarMass = 24000.0;
    double binaryStarMassEach = 12000.0;
    double trailLengthSetting = 340.0;
    double timeScale = 1.0;

    double globalTime = 0.0;

    SimulationMode mode = SimulationMode::SINGLE_STAR;

    vector<BackgroundStar> backgroundStars;
    vector<Body> stars;
    vector<Body> planets;
    vector<deque<Point>> trails;

    SDL_Rect modeButton{};
    SDL_Rect randomButton{};

    Slider massSlider;
    Slider trailSlider;
    Slider timeSlider;
};




// Drawing functions ===============================================================

static void drawCircle(SDL_Renderer* renderer, int cx, int cy, int r) {
    for (int y = -r; y <= r; y++) {
        int yy = y * y;
        int xSpan = (int)sqrt((double)(r * r - yy));
        SDL_RenderDrawLine(renderer, cx - xSpan, cy + y, cx + xSpan, cy + y);
    }
}

static bool pointInRect(int x, int y, const SDL_Rect& rect) {
    return x >= rect.x && x < rect.x + rect.w &&
           y >= rect.y && y < rect.y + rect.h;
}





// Drawing text ===============================================================

// String to texture
static SDL_Texture* createTextTexture(SDL_Renderer* renderer, TTF_Font* font, const string& text, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);

    if (!surface) {
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

// Render text
static void drawText(SDL_Renderer* renderer, TTF_Font* font, const string& text, int x, int y, SDL_Color color) {
    SDL_Texture* texture = createTextTexture(renderer, font, text, color);

    if (!texture) {
        return;
    }

    int w = 0;
    int h = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);

    SDL_Rect dst{ x, y, w, h };
    SDL_RenderCopy(renderer, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
}

// Center text inside a rectangle
static void drawCenteredText(SDL_Renderer* renderer, TTF_Font* font, const string& text, const SDL_Rect& rect, SDL_Color color) {
    int textW = 0;
    int textH = 0;

    if (TTF_SizeUTF8(font, text.c_str(), &textW, &textH) != 0) {
        return;
    }

    int x = rect.x + (rect.w - textW) / 2;
    int y = rect.y + (rect.h - textH) / 2;
    drawText(renderer, font, text, x, y, color);
}







// Visuals ============================================================

// Match planet radius to mass
static int radiusFromMass(double mass) {
    if (mass < 0.8) {
        return 4;
    } else if (mass < 1.3) {
        return 5;
    } else if (mass < 2.1) {
        return 6;
    } else if (mass < 3.2) {
        return 7;
    } else if (mass < 4.5) {
        return 8;
    } else {
        return 9;
    }
}

// Add glow around a body
static void addGlow(SDL_Renderer* renderer, int cx, int cy, int baseRadius, SDL_Color color) {
    for (int i = 5; i >= 1; i--) {
        int r = baseRadius + i * 5;
        Uint8 alpha = (Uint8)(4 + i * 7);
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, alpha);
        drawCircle(renderer, cx, cy, r);
    }
}





// UI =================================================================

// Draw a button
static void drawButton(SDL_Renderer* renderer, TTF_Font* font, const SDL_Rect& rect, const string& label, bool hover, bool accent = false) {
    if (accent) {
        if (hover) {
            SDL_SetRenderDrawColor(renderer, 66, 118, 210, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 52, 92, 166, 255);
        }
    } else {
        if (hover) {
            SDL_SetRenderDrawColor(renderer, 36, 46, 64, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 26, 34, 48, 255);
        }
    }

    SDL_RenderFillRect(renderer, &rect);

    if (accent) {
        SDL_SetRenderDrawColor(renderer, 120, 180, 255, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 70, 94, 130, 255);
    }

    SDL_RenderDrawRect(renderer, &rect);

    drawCenteredText(renderer, font, label, rect, SDL_Color{235, 242, 255, 255});
}

// Display slider value
static string sliderValueText(const Slider& slider) {
    double value = *slider.boundValue;

    if (slider.label == "mass") {
        return "Mass: " + to_string((int)round(value));
    } else if (slider.label == "trails") {
        return "Trails: " + to_string((int)round(value));
    } else {
        return "Time: " + to_string((int)round(value * 100.0)) + "%";
    }
}

// Draw slider track, fill, knob, and value
static void drawSlider(SDL_Renderer* renderer, TTF_Font* font, const Slider& slider, int mouseX, int mouseY) {
    double value = *slider.boundValue;

    // Normalize slider value so it can be mapped onto the track width
    double norm = (value - slider.minValue) / (slider.maxValue - slider.minValue);
    norm = clampDouble(norm, 0.0, 1.0);

    SDL_SetRenderDrawColor(renderer, 24, 30, 42, 255);
    SDL_RenderFillRect(renderer, &slider.trackRect);

    SDL_SetRenderDrawColor(renderer, 58, 76, 110, 255);
    SDL_RenderDrawRect(renderer, &slider.trackRect);

    SDL_Rect fill = slider.trackRect;
    fill.w = max(1, (int)round(slider.trackRect.w * norm));
    SDL_SetRenderDrawColor(renderer, 72, 132, 230, 255);
    SDL_RenderFillRect(renderer, &fill);

    int knobX = slider.trackRect.x + (int)round(norm * slider.trackRect.w);
    knobX = clampInt(knobX, slider.trackRect.x, slider.trackRect.x + slider.trackRect.w);

    SDL_Rect knobHit{
        knobX - 8,
        slider.trackRect.y - 8,
        16,
        slider.trackRect.h + 16
    };

    bool hover = pointInRect(mouseX, mouseY, knobHit) || slider.dragging;

    if (hover) {
        SDL_SetRenderDrawColor(renderer, 225, 238, 255, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 186, 210, 245, 255);
    }

    drawCircle(renderer, knobX, slider.trackRect.y + slider.trackRect.h / 2, 7);

    drawText(
        renderer,
        font,
        sliderValueText(slider),
        slider.trackRect.x,
        slider.trackRect.y + slider.trackRect.h + 10,
        SDL_Color{175, 195, 220, 255}
    );
}

static void startSliderDragIfClicked(Slider& slider, int mouseX, int mouseY) {
    SDL_Rect hit{
        slider.trackRect.x - 10,
        slider.trackRect.y - 12,
        slider.trackRect.w + 20,
        slider.trackRect.h + 32
    };

    if (pointInRect(mouseX, mouseY, hit)) {
        slider.dragging = true;
    }
}

static void stopSliderDrag(Slider& slider) {
    slider.dragging = false;
}

static void updateSliderFromMouse(Slider& slider, int mouseX) {
    if (!slider.dragging || !slider.boundValue) {
        return;
    }

    double t = (double)(mouseX - slider.trackRect.x) / (double)slider.trackRect.w;
    t = clampDouble(t, 0.0, 1.0);

    *slider.boundValue = slider.minValue + (slider.maxValue - slider.minValue) * t;
}





// Creating bodies ====================================================================================

static Body makePlanet(double x, double y, double vx, double vy, double mass, int colorR, int colorG, int colorB) {
    Body body;
    body.pos = {x, y};
    body.vel = {vx, vy};
    body.mass = mass;
    body.radius = radiusFromMass(mass);
    body.colorR = colorR;
    body.colorG = colorG;
    body.colorB = colorB;
    body.fixed = false;
    return body;
}






// Generation ==========================================================================

// Generate planets around a single system with random orbits and matching speeds
static void generateSingleStarPlanets(vector<Body>& planets, double cx, double cy, double starMass, mt19937& gen) {
    planets.clear();

    uniform_int_distribution<int> planetCountDist(3, 5);
    uniform_real_distribution<double> massDist(0.6, 4.8);
    uniform_int_distribution<int> colorDist(100, 255);
    uniform_real_distribution<double> angleDist(0.0, 2.0 * M_PI);
    uniform_real_distribution<double> speedFactorDist(0.94, 1.03);
    uniform_real_distribution<double> tangentNoiseDist(-14.0, 14.0);
    uniform_real_distribution<double> radialNoiseDist(-10.0, 10.0);

    int planetCount = planetCountDist(gen);
    const double G = 1000.0;

    double baseR = 155.0;
    double gap = 76.0;

    planets.reserve(planetCount);

    for (int i = 0; i < planetCount; i++) {
        double r = baseR + i * gap + uniform_real_distribution<double>(-10.0, 16.0)(gen);
        double angle = angleDist(gen);

        double x = cx + cos(angle) * r;
        double y = cy + sin(angle) * r;

        double v = sqrt(G * starMass / r) * speedFactorDist(gen);

        double tangentX = -sin(angle);
        double tangentY =  cos(angle);
        double radialX  =  cos(angle);
        double radialY  =  sin(angle);

        double vx = tangentX * v + tangentX * tangentNoiseDist(gen) + radialX * radialNoiseDist(gen);
        double vy = tangentY * v + tangentY * tangentNoiseDist(gen) + radialY * radialNoiseDist(gen);

        double mass = massDist(gen);

        planets.push_back(makePlanet(x, y, vx, vy, mass, colorDist(gen), colorDist(gen), colorDist(gen)));
    }
}

// Generate planets around a binary system with random orbits and matching speeds
static void generateBinaryPlanets(vector<Body>& planets, double cx, double cy, double totalMass, mt19937& gen) {
    planets.clear();

    uniform_int_distribution<int> planetCountDist(4, 6);
    uniform_real_distribution<double> massDist(0.5, 5.5);
    uniform_int_distribution<int> colorDist(100, 255);
    uniform_real_distribution<double> angleDist(0.0, 2.0 * M_PI);
    uniform_real_distribution<double> speedFactorDist(0.88, 1.03);
    uniform_real_distribution<double> tangentNoiseDist(-18.0, 18.0);
    uniform_real_distribution<double> radialNoiseDist(-18.0, 18.0);
    uniform_real_distribution<double> yOffsetDist(-22.0, 22.0);

    int planetCount = planetCountDist(gen);
    const double G = 1000.0;

    double startR = 300.0;
    double gap = 72.0;

    planets.reserve(planetCount);

    for (int i = 0; i < planetCount; i++) {
        double r = startR + i * gap + uniform_real_distribution<double>(-16.0, 18.0)(gen);
        double angle = angleDist(gen);

        double x = cx + cos(angle) * r;
        double y = cy + sin(angle) * r + yOffsetDist(gen);

        double v = sqrt(G * totalMass / r) * speedFactorDist(gen);

        double tangentX = -sin(angle);
        double tangentY =  cos(angle);
        double radialX  =  cos(angle);
        double radialY  =  sin(angle);

        double vx = tangentX * v + tangentX * tangentNoiseDist(gen) + radialX * radialNoiseDist(gen);
        double vy = tangentY * v + tangentY * tangentNoiseDist(gen) + radialY * radialNoiseDist(gen);

        double mass = massDist(gen);

        planets.push_back(makePlanet(x, y, vx, vy, mass, colorDist(gen), colorDist(gen), colorDist(gen)));
    }
}

static void resetSimulation(AppState& app, mt19937& gen) {
    app.stars.clear();
    app.planets.clear();
    app.trails.clear();

    double centerX = app.windowWidth / 2.0;
    double centerY = (app.windowHeight - app.panelHeight) / 2.0 - 12.0;

    if (app.mode == SimulationMode::SINGLE_STAR) {
        Body star;
        star.pos = {centerX, centerY};
        star.vel = {0.0, 0.0};
        star.mass = app.singleStarMass;
        star.radius = 18;
        star.colorR = 255;
        star.colorG = 224;
        star.colorB = 96;
        star.fixed = true;

        app.stars.push_back(star);

        generateSingleStarPlanets(app.planets, centerX, centerY, app.singleStarMass, gen);
    } else {
        double separation = 165.0;
        double G = 1000.0;
        double mass = app.binaryStarMassEach;

        double orbitSpeed = sqrt(G * mass / (2.0 * separation));

        Body star1;
        star1.pos = {centerX - separation / 2.0, centerY};
        star1.vel = {0.0, orbitSpeed};
        star1.mass = mass;
        star1.radius = 14;
        star1.colorR = 255;
        star1.colorG = 210;
        star1.colorB = 70;
        star1.fixed = false;

        Body star2;
        star2.pos = {centerX + separation / 2.0, centerY};
        star2.vel = {0.0, -orbitSpeed};
        star2.mass = mass;
        star2.radius = 14;
        star2.colorR = 255;
        star2.colorG = 242;
        star2.colorB = 150;
        star2.fixed = false;

        app.stars.push_back(star1);
        app.stars.push_back(star2);

        generateBinaryPlanets(app.planets, centerX, centerY, 2.0 * mass, gen);
    }

    app.trails.resize(app.planets.size());
}




// Background ================================================================================

static void generateBackgroundStars(vector<BackgroundStar>& background, int width, int height, mt19937& gen) {
    background.clear();

    uniform_real_distribution<float> xDist(0.0f, (float)width);
    uniform_real_distribution<float> yDist(0.0f, (float)height);
    uniform_real_distribution<float> sizeDist(1.0f, 2.2f);
    uniform_real_distribution<float> phaseDist(0.0f, 6.28318f);
    uniform_int_distribution<int> alphaDist(40, 160);

    background.reserve(240);

    for (int i = 0; i < 240; i++) {
        BackgroundStar star;
        star.x = xDist(gen);
        star.y = yDist(gen);
        star.size = sizeDist(gen);
        star.phase = phaseDist(gen);
        star.alpha = (Uint8)alphaDist(gen);
        background.push_back(star);
    }
}

static void drawBackground(SDL_Renderer* renderer, const vector<BackgroundStar>& background, double timeSeconds, int width, int height) {
    for (int y = 0; y < height; y++) {
        double t = (double)y / (double)height;
        int r = lerpInt(6, 12, t);
        int g = lerpInt(9, 14, t);
        int b = lerpInt(18, 30, t);

        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderDrawLine(renderer, 0, y, width, y);
    }

    for (const auto& star : background) {
        double pulse = 0.7 + 0.3 * sin(timeSeconds * 0.7 + star.phase);
        Uint8 alpha = (Uint8)clampInt((int)(star.alpha * pulse), 20, 180);

        SDL_SetRenderDrawColor(renderer, 180, 205, 255, alpha);
        drawCircle(renderer, (int)star.x, (int)star.y, (int)star.size);
    }
}





// Physics and orbital motion =========================================================================

static Vec2 accelerationTowards(const Body& source, const Body& target, double G, double softening) {
    Vec2 acc{0.0, 0.0};

    double dx = source.pos.x - target.pos.x;
    double dy = source.pos.y - target.pos.y;

    double dist2 = dx * dx + dy * dy + softening;
    double dist = sqrt(dist2);

    double a = G * source.mass / dist2;
    double nx = dx / dist;
    double ny = dy / dist;

    acc.x = a * nx;
    acc.y = a * ny;
    return acc;
}

static void applyAccelerationAndMove(Body& body, const Vec2& acc, double dt) {
    if (body.fixed) {
        return;
    }

    body.vel.x += acc.x * dt;
    body.vel.y += acc.y * dt;

    body.pos.x += body.vel.x * dt;
    body.pos.y += body.vel.y * dt;
}

static void keepTrailSize(deque<Point>& trail, int maxTrailPoints) {
    while ((int)trail.size() > maxTrailPoints) {
        trail.pop_front();
    }
}

static void updateStarMasses(AppState& app) {
    if (app.mode == SimulationMode::SINGLE_STAR) {
        if (!app.stars.empty()) {
            app.stars[0].mass = app.singleStarMass;
        }
    } else {
        for (auto& star : app.stars) {
            star.mass = app.binaryStarMassEach;
        }
    }
}

static void updateSimulation(AppState& app, double dt) {
    const double G = 1000.0;
    const double softStars = 90.0;
    const double softPlanetsToStars = 82.0;
    const double softPlanetsToPlanets = 48.0;
    const int substeps = 4;

    double scaledDt = dt * app.timeScale;
    double stepDt = scaledDt / substeps;
    int maxTrailPoints = (int)round(app.trailLengthSetting);

    updateStarMasses(app);

    for (int step = 0; step < substeps; ++step) {
        vector<Vec2> starAccelerations(app.stars.size(), Vec2{0.0, 0.0});

        for (size_t i = 0; i < app.stars.size(); i++) {
            if (app.stars[i].fixed) {
                continue;
            }

            for (size_t j = 0; j < app.stars.size(); j++) {
                if (i == j) {
                    continue;
                }

                Vec2 acc = accelerationTowards(app.stars[j], app.stars[i], G, softStars);
                starAccelerations[i].x += acc.x;
                starAccelerations[i].y += acc.y;
            }
        }

        for (size_t i = 0; i < app.stars.size(); i++) {
            applyAccelerationAndMove(app.stars[i], starAccelerations[i], stepDt);
        }

        vector<Vec2> planetAccelerations(app.planets.size(), Vec2{0.0, 0.0});

        for (size_t i = 0; i < app.planets.size(); i++) {
            for (const Body& star : app.stars) {
                Vec2 acc = accelerationTowards(star, app.planets[i], G, softPlanetsToStars);
                planetAccelerations[i].x += acc.x;
                planetAccelerations[i].y += acc.y;
            }

            for (size_t j = 0; j < app.planets.size(); j++) {
                if (i == j) {
                    continue;
                }

                Vec2 acc = accelerationTowards(app.planets[j], app.planets[i], G, softPlanetsToPlanets);
                planetAccelerations[i].x += acc.x;
                planetAccelerations[i].y += acc.y;
            }
        }

        for (size_t i = 0; i < app.planets.size(); i++) {
            applyAccelerationAndMove(app.planets[i], planetAccelerations[i], stepDt);

            app.trails[i].push_back({app.planets[i].pos.x, app.planets[i].pos.y});
            keepTrailSize(app.trails[i], maxTrailPoints);
        }
    }
}





// Rendering ==========================================================================

static void drawTrails(SDL_Renderer* renderer, const AppState& app) {
    for (size_t i = 0; i < app.planets.size(); i++) {
        const Body& planet = app.planets[i];
        const auto& trail = app.trails[i];

        if (trail.size() < 2) {
            continue;
        }

        int trailR = max(30, planet.colorR - 28);
        int trailG = max(30, planet.colorG - 28);
        int trailB = max(40, planet.colorB - 10);

        int idx = 0;
        int count = (int)trail.size();

        auto it = trail.begin();
        auto prev = it++;

        for (; it != trail.end(); ++it, ++prev, ++idx) {
            float t = (float)(idx + 1) / (float)max(1, count - 1);
            Uint8 alpha = (Uint8)(10 + t * 120.0f);

            SDL_SetRenderDrawColor(renderer, trailR, trailG, trailB, alpha);
            SDL_RenderDrawLine(
                renderer,
                (int)round(prev->x), (int)round(prev->y),
                (int)round(it->x),   (int)round(it->y)
            );
        }
    }
}

// Draw stars and planets
static void drawBodies(SDL_Renderer* renderer, const AppState& app) {
    for (const Body& star : app.stars) {
        SDL_Color glowColor{(Uint8)star.colorR, (Uint8)star.colorG, (Uint8)star.colorB, 255};
        addGlow(renderer, (int)round(star.pos.x), (int)round(star.pos.y), star.radius, glowColor);
    }

    for (const Body& planet : app.planets) {
        SDL_SetRenderDrawColor(renderer, planet.colorR, planet.colorG, planet.colorB, 255);
        drawCircle(renderer, (int)round(planet.pos.x), (int)round(planet.pos.y), planet.radius);
    }

    for (const Body& star : app.stars) {
        SDL_SetRenderDrawColor(renderer, star.colorR, star.colorG, star.colorB, 255);
        drawCircle(renderer, (int)round(star.pos.x), (int)round(star.pos.y), star.radius);
    }
}

// Draw info
static void drawTopLabels(SDL_Renderer* renderer, TTF_Font* titleFont, TTF_Font* font, const AppState& app) {
    drawText(renderer, titleFont, "GravTrails Engine", 20, 18, SDL_Color{232, 240, 255, 255});

    string modeText;

    if (app.mode == SimulationMode::SINGLE_STAR) {
        modeText = "single star system";
    } else {
        modeText = "binary star system";
    }

    drawText(renderer, font, modeText, 22, 50, SDL_Color{120, 182, 255, 255});
}

static void drawBottomPanel(SDL_Renderer* renderer, TTF_Font* font, const AppState& app, int mouseX, int mouseY, double rawDt) {
    SDL_SetRenderDrawColor(renderer, 10, 14, 22, 248);

    SDL_Rect panel{0, app.windowHeight - app.panelHeight, app.windowWidth, app.panelHeight};
    SDL_RenderFillRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, 28, 48, 78, 255);
    SDL_RenderDrawLine(renderer, 0, app.windowHeight - app.panelHeight, app.windowWidth, app.windowHeight - app.panelHeight);

    bool hoverMode = pointInRect(mouseX, mouseY, app.modeButton);
    bool hoverRandom = pointInRect(mouseX, mouseY, app.randomButton);

    string modeButtonLabel;

    if (app.mode == SimulationMode::SINGLE_STAR) {
        modeButtonLabel = "Binary system";
    } else {
        modeButtonLabel = "Single system";
    }

    drawButton(renderer, font, app.modeButton, modeButtonLabel, hoverMode, true);
    drawButton(renderer, font, app.randomButton, "New", hoverRandom, false);

    drawSlider(renderer, font, app.massSlider, mouseX, mouseY);
    drawSlider(renderer, font, app.trailSlider, mouseX, mouseY);
    drawSlider(renderer, font, app.timeSlider, mouseX, mouseY);

    int currentMass = 0;

    if (app.mode == SimulationMode::SINGLE_STAR) {
        currentMass = (int)round(app.singleStarMass);
    } else {
        currentMass = (int)round(app.binaryStarMassEach);
    }

    int fps = 0;

    if (rawDt > 0.000001) {
        fps = (int)round(1.0 / rawDt);
    } else {
        fps = 0;
    }

    string stats = "Planets: " + to_string((int)app.planets.size()) +
                   "   Stars: " + to_string((int)app.stars.size()) +
                   "   FPS: " + to_string(fps) +
                   "   Mass: " + to_string(currentMass);

    drawText(renderer, font, stats, 20, app.windowHeight - app.panelHeight + 74, SDL_Color{145, 165, 195, 255});
}

// Render one full frame
static void renderScene(SDL_Renderer* renderer, TTF_Font* titleFont, TTF_Font* font, const AppState& app, int mouseX, int mouseY, double rawDt) {
    int simHeight = app.windowHeight - app.panelHeight;

    drawBackground(renderer, app.backgroundStars, app.globalTime, app.windowWidth, simHeight);
    drawTrails(renderer, app);
    drawBodies(renderer, app);
    drawTopLabels(renderer, titleFont, font, app);
    drawBottomPanel(renderer, font, app, mouseX, mouseY, rawDt);

    SDL_RenderPresent(renderer);
}






// Setup ==============================================================

// Look for fonts on different operating systems
static bool loadFonts(TTF_Font*& font, TTF_Font*& titleFont) {
    const char* fontPaths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/Library/Fonts/Arial.ttf"
    };

    for (const char* path : fontPaths) {
        font = TTF_OpenFont(path, 17);
        titleFont = TTF_OpenFont(path, 24);

        if (font && titleFont) {
            return true;
        }

        if (font) {
            TTF_CloseFont(font);
            font = nullptr;
        }

        if (titleFont) {
            TTF_CloseFont(titleFont);
            titleFont = nullptr;
        }
    }

    return false;
}

static void setupUI(AppState& app) {
    app.modeButton = SDL_Rect{20,  app.windowHeight - app.panelHeight + 22, 190, 34};
    app.randomButton = SDL_Rect{220, app.windowHeight - app.panelHeight + 22, 140, 34};

    app.massSlider.trackRect = SDL_Rect{390, app.windowHeight - app.panelHeight + 28, 200, 8};
    app.massSlider.minValue = 8000.0;
    app.massSlider.maxValue = 80000.0;
    app.massSlider.boundValue = &app.singleStarMass;
    app.massSlider.label = "mass";

    app.trailSlider.trackRect = SDL_Rect{640, app.windowHeight - app.panelHeight + 28, 180, 8};
    app.trailSlider.minValue = 120.0;
    app.trailSlider.maxValue = 520.0;
    app.trailSlider.boundValue = &app.trailLengthSetting;
    app.trailSlider.label = "trails";

    app.timeSlider.trackRect = SDL_Rect{870, app.windowHeight - app.panelHeight + 28, 180, 8};
    app.timeSlider.minValue = 0.35;
    app.timeSlider.maxValue = 1.85;
    app.timeSlider.boundValue = &app.timeScale;
    app.timeSlider.label = "time";
}

static void updateMassSliderBinding(AppState& app) {
    if (app.mode == SimulationMode::SINGLE_STAR) {
        app.massSlider.boundValue = &app.singleStarMass;
    } else {
        app.massSlider.boundValue = &app.binaryStarMassEach;
    }
}






// User input ========================================================

static void toggleMode(AppState& app, mt19937& gen) {
    if (app.mode == SimulationMode::SINGLE_STAR) {
        app.mode = SimulationMode::DOUBLE_STAR;
    } else {
        app.mode = SimulationMode::SINGLE_STAR;
    }

    resetSimulation(app, gen);
}

static void processMouseDown(AppState& app, mt19937& gen, int mouseX, int mouseY) {
    if (pointInRect(mouseX, mouseY, app.modeButton)) {
        toggleMode(app, gen);
        return;
    } else if (pointInRect(mouseX, mouseY, app.randomButton)) {
        resetSimulation(app, gen);
        return;
    }

    startSliderDragIfClicked(app.massSlider, mouseX, mouseY);
    startSliderDragIfClicked(app.trailSlider, mouseX, mouseY);
    startSliderDragIfClicked(app.timeSlider, mouseX, mouseY);
}

static void processMouseUp(AppState& app) {
    stopSliderDrag(app.massSlider);
    stopSliderDrag(app.trailSlider);
    stopSliderDrag(app.timeSlider);
}

static void processMouseMotion(AppState& app, int mouseX) {
    updateSliderFromMouse(app.massSlider, mouseX);
    updateSliderFromMouse(app.trailSlider, mouseX);
    updateSliderFromMouse(app.timeSlider, mouseX);
}

static void handleEvents(bool& running, AppState& app, mt19937& gen) {
    SDL_Event e;

    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            running = false;
        } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            running = false;
        } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            processMouseDown(app, gen, e.button.x, e.button.y);
        } else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
            processMouseUp(app);
        } else if (e.type == SDL_MOUSEMOTION) {
            processMouseMotion(app, e.motion.x);
        }
    }
}






// Main ================================================================================

int main() {
    random_device rd;
    mt19937 gen(rd());

    AppState app;
    setupUI(app);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        cout << "SDL init error: " << SDL_GetError() << endl;
        return 1;
    }

    if (TTF_Init() != 0) {
        cout << "TTF init error: " << TTF_GetError() << endl;
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "GravTrails Engine",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        app.windowWidth, app.windowHeight,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        cout << "Window error: " << SDL_GetError() << endl;
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!renderer) {
        cout << "Renderer error: " << SDL_GetError() << endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    TTF_Font* font = nullptr;
    TTF_Font* titleFont = nullptr;

    if (!loadFonts(font, titleFont)) {
        cout << "Cannot load font. Update fontPaths." << endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    int simHeight = app.windowHeight - app.panelHeight;
    generateBackgroundStars(app.backgroundStars, app.windowWidth, simHeight, gen);
    resetSimulation(app, gen);

    uint64_t previousCounter = SDL_GetPerformanceCounter();
    double performanceFrequency = (double)SDL_GetPerformanceFrequency();

    bool running = true;

    while (running) {
        uint64_t nowCounter = SDL_GetPerformanceCounter();

        double rawDt = (nowCounter - previousCounter) / performanceFrequency;
        previousCounter = nowCounter;

        double simDt = rawDt;

        if (simDt > 0.04) {
            simDt = 0.04;
        }

        app.globalTime += rawDt;

        updateMassSliderBinding(app);

        int mouseX = 0;
        int mouseY = 0;
        SDL_GetMouseState(&mouseX, &mouseY);

        handleEvents(running, app, gen);
        updateSimulation(app, simDt);
        renderScene(renderer, titleFont, font, app, mouseX, mouseY, rawDt);
    }

    TTF_CloseFont(font);
    TTF_CloseFont(titleFont);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
