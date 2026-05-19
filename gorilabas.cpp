// Este codigo sigue la senda del ingeniero

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <random>
#include <sstream>
#include <string>
#include <vector>

struct Building {
    RECT rect;
    COLORREF color;
    std::vector<int> columnTops;
    int glowOffset;
};

struct Player {
    POINT position;
    COLORREF color;
    bool alive;
};

struct Projectile {
    bool active = false;
    double x = 0.0;
    double y = 0.0;
    double vx = 0.0;
    double vy = 0.0;
    int power = 60;
};

struct Explosion {
    bool active = false;
    int x = 0;
    int y = 0;
    int radius = 0;
    int maxRadius = 0;
    int framesLeft = 0;
    bool resetRoundAfter = false;
};

struct PendingRound {
    bool active = false;
    int winner = -1;
};

struct GameState {
    std::vector<Building> buildings;
    Player players[2];
    Projectile projectile;
    Explosion explosion;
    PendingRound pendingRound;
    int scores[2] = {0, 0};
    int currentPlayer = 0;
    int angle = 45;
    int power = 60;
    int wind = 0;
    int animationTick = 0;
    bool roundOver = false;
    std::string status = "Flechas arriba/abajo: angulo. Izquierda/derecha: potencia. Espacio: lanzar.";
};

static const int WINDOW_WIDTH = 1100;
static const int WINDOW_HEIGHT = 700;
static const int GROUND_Y = 620;
static const int PLAYER_RADIUS = 18;
static const int COLUMN_WIDTH = 8;
static const double GRAVITY = 950.0;
static const double TIME_STEP = 1.0 / 60.0;

static GameState game;
static std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));

int randomInt(int minValue, int maxValue) {
    std::uniform_int_distribution<int> dist(minValue, maxValue);
    return dist(rng);
}

double degToRad(double angle) {
    return angle * 3.14159265358979323846 / 180.0;
}

void updateStatus(const std::string& text) {
    game.status = text;
}

void generateCity() {
    game.buildings.clear();
    static const COLORREF palette[] = {
        RGB(214, 0, 86),
        RGB(168, 168, 168),
        RGB(27, 178, 191),
        RGB(211, 0, 84),
        RGB(166, 166, 166)
    };

    int x = 0;
    while (x < WINDOW_WIDTH) {
        int width = randomInt(100, 180);
        if (x + width > WINDOW_WIDTH) {
            width = WINDOW_WIDTH - x;
        }

        width -= width % COLUMN_WIDTH;
        if (width < COLUMN_WIDTH) {
            width = COLUMN_WIDTH;
        }

        int height = randomInt(250, 520);
        RECT rect;
        rect.left = x;
        rect.right = x + width;
        rect.bottom = GROUND_Y;
        rect.top = GROUND_Y - height;

        Building building;
        building.rect = rect;
        building.color = palette[randomInt(0, 4)];
        building.glowOffset = randomInt(0, 3);

        int columns = width / COLUMN_WIDTH;
        for (int i = 0; i < columns; ++i) {
            building.columnTops.push_back(rect.top);
        }

        game.buildings.push_back(building);
        x += width;
    }
}

Building& buildingAtIndex(size_t index) {
    return game.buildings[index];
}

POINT buildingTopCenter(const Building& building) {
    POINT p;
    p.x = (building.rect.left + building.rect.right) / 2;
    int centerColumn = static_cast<int>(building.columnTops.size() / 2);
    p.y = building.columnTops[centerColumn];
    return p;
}

void placePlayers() {
    size_t leftIndex = std::max<size_t>(1, game.buildings.size() / 6);
    size_t rightIndex = std::min(game.buildings.size() - 2, game.buildings.size() * 5 / 6);

    game.players[0] = {buildingTopCenter(buildingAtIndex(leftIndex)), RGB(255, 209, 102), true};
    game.players[1] = {buildingTopCenter(buildingAtIndex(rightIndex)), RGB(139, 233, 253), true};
}

void resetRound() {
    generateCity();
    placePlayers();
    game.projectile = {};
    game.explosion = {};
    game.pendingRound = {};
    game.currentPlayer = randomInt(0, 1);
    game.angle = 45;
    game.power = 60;
    game.wind = randomInt(-20, 20);
    game.roundOver = false;
    updateStatus("Nueva ciudad lista. Ajusta angulo y potencia, luego presiona Espacio.");
}

void startNewMatch() {
    game.scores[0] = 0;
    game.scores[1] = 0;
    resetRound();
}

RECT playerBounds(const Player& player) {
    RECT rect;
    rect.left = player.position.x - PLAYER_RADIUS;
    rect.right = player.position.x + PLAYER_RADIUS;
    rect.top = player.position.y - PLAYER_RADIUS * 2;
    rect.bottom = player.position.y + PLAYER_RADIUS;
    return rect;
}

bool pointInRect(int x, int y, const RECT& rect) {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

int buildingTopAtX(const Building& building, int x) {
    int localX = std::clamp<int>(x - building.rect.left, 0, static_cast<int>(building.rect.right - building.rect.left - 1));
    int index = std::clamp(localX / COLUMN_WIDTH, 0, static_cast<int>(building.columnTops.size()) - 1);
    return building.columnTops[index];
}

bool pointHitsBuilding(const Building& building, int x, int y) {
    if (x < building.rect.left || x >= building.rect.right || y > building.rect.bottom) {
        return false;
    }

    return y >= buildingTopAtX(building, x);
}

void switchTurn() {
    game.currentPlayer = 1 - game.currentPlayer;
    game.angle = 45;
    game.power = 60;
}

void launchProjectile() {
    if (game.projectile.active || game.roundOver) {
        return;
    }

    Player& shooter = game.players[game.currentPlayer];
    Player& target = game.players[1 - game.currentPlayer];
    double direction = (shooter.position.x < target.position.x) ? 1.0 : -1.0;
    double radians = degToRad(static_cast<double>(game.angle));
    double speed = 280.0 +  game.power * 8.0;

    game.projectile.active = true;
    game.projectile.x = shooter.position.x + direction * 26.0;
    game.projectile.y = shooter.position.y - 42.0;
    game.projectile.vx = std::cos(radians) * speed * direction;
    game.projectile.vy = -std::sin(radians) * speed;
    game.projectile.power = game.power;

    std::ostringstream oss;
    oss << "Jugador " << (game.currentPlayer + 1) << " lanzo con angulo " << game.angle
        << " y potencia " << game.power << ".";
    updateStatus(oss.str());
}

void settlePlayers() {
    for (auto& player : game.players) {
        if (!player.alive) {
            continue;
        }

        for (const auto& building : game.buildings) {
            if (player.position.x >= building.rect.left && player.position.x <= building.rect.right) {
                player.position.y = buildingTopAtX(building, player.position.x);
                if (building.rect.bottom - player.position.y < 45) {
                    player.alive = false;
                }
                break;
            }
        }
    }
}

void damageBuildings(int x, int y, int blastRadius) {
    for (auto& building : game.buildings) {
        if (x < building.rect.left - blastRadius || x > building.rect.right + blastRadius) {
            continue;
        }

        for (size_t i = 0; i < building.columnTops.size(); ++i) {
            int columnCenter = building.rect.left + static_cast<int>(i) * COLUMN_WIDTH + COLUMN_WIDTH / 2;
            int dx = std::abs(columnCenter - x);
            if (dx > blastRadius) {
                continue;
            }

            double circleDepth = std::sqrt(static_cast<double>(blastRadius * blastRadius - dx * dx));
            int carve = std::max(6, static_cast<int>(circleDepth * 0.72));
            building.columnTops[i] = std::min(GROUND_Y - 22, building.columnTops[i] + carve);
        }

        int minTop = GROUND_Y;
        for (int top : building.columnTops) {
            minTop = std::min(minTop, top);
        }
        building.rect.top = minTop;
    }

    for (auto& player : game.players) {
        if (!player.alive) {
            continue;
        }

        double dx = static_cast<double>(player.position.x - x);
        double dy = static_cast<double>((player.position.y - PLAYER_RADIUS) - y);
        if (std::sqrt(dx * dx + dy * dy) <= blastRadius + PLAYER_RADIUS * 0.8) {
            player.alive = false;
        }
    }

    settlePlayers();
}

void beginExplosion(int x, int y, bool resetRoundAfter, int winner) {
    int blastRadius = std::clamp(14 + game.projectile.power / 2, 22, 70);
    game.projectile.active = false;
    game.explosion.active = true;
    game.explosion.x = x;
    game.explosion.y = y;
    game.explosion.radius = 6;
    game.explosion.maxRadius = blastRadius;
    game.explosion.framesLeft = 18;
    game.explosion.resetRoundAfter = resetRoundAfter;
    game.pendingRound.active = resetRoundAfter;
    game.pendingRound.winner = winner;
    damageBuildings(x, y, blastRadius);
}

void resolveExplosion(int x, int y) {
    int blastRadius = std::clamp(14 + game.projectile.power / 2, 22, 70);
    bool shooterWillDie = false;
    bool opponentWillDie = false;

    for (int i = 0; i < 2; ++i) {
        const Player& player = game.players[i];
        if (!player.alive) {
            continue;
        }

        double dx = static_cast<double>(player.position.x - x);
        double dy = static_cast<double>((player.position.y - PLAYER_RADIUS) - y);
        bool willDie = std::sqrt(dx * dx + dy * dy) <= blastRadius + PLAYER_RADIUS * 0.8;

        if (i == game.currentPlayer) {
            shooterWillDie = willDie;
        } else {
            opponentWillDie = willDie;
        }
    }

    beginExplosion(x, y, opponentWillDie && !shooterWillDie, game.currentPlayer);

    const Player& shooter = game.players[game.currentPlayer];
    const Player& opponent = game.players[1 - game.currentPlayer];
    if (!opponent.alive && shooter.alive) {
        game.scores[game.currentPlayer] += 1;
       
if (game.scores[0] >= 3 || game.scores[1] >= 3) {
    game.roundOver = true;

    if (game.scores[0] > game.scores[1]) {
        updateStatus("Jugador 1 gana la partida.");
    } else {
        updateStatus("Jugador 2 gana la partida.");
    }
}
        std::ostringstream oss;
        oss << "Jugador " << (game.currentPlayer + 1)
            << " gano la ronda.";
        updateStatus(oss.str());
    } else if (!shooter.alive && opponent.alive) {
        game.scores[1 - game.currentPlayer] += 1;
        game.pendingRound.active = true;
        game.pendingRound.winner = 1 - game.currentPlayer;
        std::ostringstream oss;
        oss << "Jugador " << (2 - game.currentPlayer)
            << " gano la ronda.";
        updateStatus(oss.str());
    } else if (!shooter.alive && !opponent.alive) {
        game.pendingRound.active = true;
        game.pendingRound.winner = -1;
        updateStatus("Empate por explosion. Nueva ciudad en camino.");
    } else {
        switchTurn();
        std::ostringstream oss;
        oss << "No hubo impacto directo. Turno del Jugador " << (game.currentPlayer + 1) << ".";
        updateStatus(oss.str());
    }
}

void updateSimulation() {
    if (game.explosion.active) {
        game.explosion.radius = std::min(game.explosion.maxRadius, game.explosion.radius + 4);
        game.explosion.framesLeft -= 1;
        if (game.explosion.framesLeft <= 0) {
            game.explosion.active = false;
            if (game.pendingRound.active) {
                game.pendingRound.active = false;
                resetRound();
                updateStatus("Nueva ciudad lista. Ajusta angulo y potencia, luego presiona Espacio.");
            }
        }
        return;
    }

    if (!game.projectile.active) {
        return;
    }

    game.projectile.vx += static_cast<double>(game.wind) * 0.6;
    game.projectile.vy += GRAVITY * TIME_STEP;

    double nextX = game.projectile.x + game.projectile.vx * TIME_STEP;
    double nextY = game.projectile.y + game.projectile.vy * TIME_STEP;

    if (nextX < -20 || nextX > WINDOW_WIDTH + 20 || nextY > WINDOW_HEIGHT + 20) {
        game.projectile.active = false;
        switchTurn();
        std::ostringstream oss;
        oss << "La banana salio del mapa. Turno del Jugador " << (game.currentPlayer + 1) << ".";
        updateStatus(oss.str());
        return;
    }

    for (const auto& building : game.buildings) {
        if (pointHitsBuilding(building, static_cast<int>(nextX), static_cast<int>(nextY))) {
            resolveExplosion(static_cast<int>(nextX), static_cast<int>(nextY));
            return;
        }
    }

    for (const auto& player : game.players) {
        if (!player.alive) {
            continue;
        }

        RECT bounds = playerBounds(player);
        if (pointInRect(static_cast<int>(nextX), static_cast<int>(nextY), bounds)) {
            resolveExplosion(static_cast<int>(nextX), static_cast<int>(nextY));
            return;
        }
    }

    game.projectile.x = nextX;
    game.projectile.y = nextY;
}

void drawTextLine(HDC hdc, int x, int y, const std::string& text, int size, COLORREF color, int weight = FW_NORMAL) {
    HFONT font = CreateFontA(size, 0, 0, 0, weight, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN,
        "Consolas");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    TextOutA(hdc, x, y, text.c_str(), static_cast<int>(text.size()));
    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void fillCircle(HDC hdc, int cx, int cy, int radius, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, brush));
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    Ellipse(hdc, cx - radius, cy - radius, cx + radius, cy + radius);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void fillEllipseColor(HDC hdc, int left, int top, int right, int bottom, COLORREF fill, COLORREF stroke, int strokeWidth = 1) {
    HBRUSH brush = CreateSolidBrush(fill);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, brush));
    HPEN pen = CreatePen(PS_SOLID, strokeWidth, stroke);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    Ellipse(hdc, left, top, right, bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void drawThickSegment(HDC hdc, int x1, int y1, int x2, int y2, int width, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    MoveToEx(hdc, x1, y1, nullptr);
    LineTo(hdc, x2, y2);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    fillCircle(hdc, x1, y1, width / 2, color);
    fillCircle(hdc, x2, y2, width / 2, color);
}

void fillRectColor(HDC hdc, int left, int top, int right, int bottom, COLORREF color) {
    RECT rect = {left, top, right, bottom};
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void drawPixelSprite(HDC hdc, int originX, int originY, int pixelSize, const std::vector<std::string>& rows, bool flipHorizontal = false) {
    for (int row = 0; row < static_cast<int>(rows.size()); ++row) {
        for (int col = 0; col < static_cast<int>(rows[row].size()); ++col) {
            int sourceCol = flipHorizontal ? static_cast<int>(rows[row].size()) - 1 - col : col;
            COLORREF color;
            bool shouldDraw = true;

            switch (rows[row][sourceCol]) {
                case 'O':
                    color = RGB(132, 72, 38);
                    break;
                case 'B':
                    color = RGB(232, 181, 126);
                    break;
                case 'F':
                    color = RGB(247, 221, 178);
                    break;
                case 'D':
                    color = RGB(72, 43, 29);
                    break;
                case 'Y':
                    color = RGB(255, 236, 38);
                    break;
                case 'S':
                    color = RGB(255, 244, 130);
                    break;
                case 'G':
                    color = RGB(52, 184, 74);
                    break;
                case 'L':
                    color = RGB(126, 232, 110);
                    break;
                default:
                    shouldDraw = false;
                    color = RGB(0, 0, 0);
                    break;
            }

            if (shouldDraw) {
                fillRectColor(
                    hdc,
                    originX + col * pixelSize,
                    originY + row * pixelSize,
                    originX + (col + 1) * pixelSize,
                    originY + (row + 1) * pixelSize,
                    color
                );
            }
        }
    }
}

void drawPolygonColor(HDC hdc, const std::vector<POINT>& points, COLORREF fill, COLORREF stroke, int strokeWidth = 1) {
    if (points.empty()) {
        return;
    }

    HBRUSH brush = CreateSolidBrush(fill);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, brush));
    HPEN pen = CreatePen(PS_SOLID, strokeWidth, stroke);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    Polygon(hdc, points.data(), static_cast<int>(points.size()));
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void drawPolylineColor(HDC hdc, const std::vector<POINT>& points, COLORREF color, int width) {
    if (points.size() < 2) {
        return;
    }

    HPEN pen = CreatePen(PS_SOLID, width, color);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    MoveToEx(hdc, points[0].x, points[0].y, nullptr);
    for (size_t i = 1; i < points.size(); ++i) {
        LineTo(hdc, points[i].x, points[i].y);
    }
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void drawBananaShape(HDC hdc, int centerX, int centerY, double scale, bool flipHorizontal = false) {
    auto pointAt = [&](int dx, int dy) {
        POINT point;
        point.x = static_cast<LONG>(std::lround(centerX + (flipHorizontal ? -dx : dx) * scale));
        point.y = static_cast<LONG>(std::lround(centerY + dy * scale));
        return point;
    };

    std::vector<POINT> body = {
        pointAt(-24, -2), pointAt(-20, -8), pointAt(-13, -12), pointAt(-4, -14),
        pointAt(6, -13), pointAt(14, -9), pointAt(20, -3), pointAt(24, 4),
        pointAt(20, 7), pointAt(12, 6), pointAt(4, 3), pointAt(-2, 2),
        pointAt(-8, 3), pointAt(-14, 7), pointAt(-19, 11), pointAt(-23, 8),
        pointAt(-20, 3)
    };

    std::vector<POINT> stem = {
        pointAt(-22, -7), pointAt(-18, -15), pointAt(-14, -14), pointAt(-16, -7)
    };

    std::vector<POINT> highlight = {
        pointAt(-14, -7), pointAt(-7, -10), pointAt(1, -10), pointAt(9, -7), pointAt(14, -3)
    };

    drawPolygonColor(hdc, body, RGB(52, 184, 74), RGB(18, 102, 36), 2);
    drawPolygonColor(hdc, stem, RGB(86, 58, 24), RGB(60, 40, 18), 1);
    drawPolylineColor(hdc, highlight, RGB(152, 242, 136), std::max(1, static_cast<int>(std::lround(scale * 2.0))));
}

void drawSunAndBanana(HDC hdc) {
    const std::vector<std::string> sun = {
        "...Y...Y...",
        "....YYY....",
        ".YSSSSSSSY.",
        "..SSDDDDSS.",
        "YYSDSSDSYY.",
        "..SSDDDDSS.",
        ".YSSSSSSSY.",
        "....YYY....",
        "...Y...Y..."
    };

    drawPixelSprite(hdc, WINDOW_WIDTH / 2 - 28, 10, 4, sun);
    drawBananaShape(hdc, WINDOW_WIDTH / 2 + 160, 72, 1.35, false);
}

void drawPlayer(HDC hdc, const Player& player) {
    if (!player.alive) {
        return;
    }
    int x = player.position.x;
    int y = player.position.y;
    int dir = (player.position.x > WINDOW_WIDTH / 2) ? -1 : 1;

    COLORREF outline = RGB(0, 0, 0);
    COLORREF fur = RGB(18, 18, 18);
    COLORREF chest = RGB(42, 42, 42);
    COLORREF face = RGB(62, 62, 62);

    fillEllipseColor(hdc, x - 12, y - 78, x + 12, y - 52, fur, outline, 2);
    fillEllipseColor(hdc, x - 9, y - 74, x + 9, y - 57, face, outline, 1);
    fillCircle(hdc, x - 4, y - 68, 1, RGB(210, 210, 210));
    fillCircle(hdc, x + 4, y - 68, 1, RGB(210, 210, 210));
    fillRectColor(hdc, x - 5, y - 63, x + 5, y - 61, RGB(90, 90, 90));

    fillEllipseColor(hdc, x - 25, y - 58, x + 25, y - 10, fur, outline, 2);
    fillEllipseColor(hdc, x - 14, y - 47, x + 14, y - 18, chest, outline, 1);

    drawThickSegment(hdc, x - 17, y - 42, x - 28, y - 16, 12, outline);
    drawThickSegment(hdc, x - 17, y - 42, x - 28, y - 16, 8, fur);
    drawThickSegment(hdc, x - 28, y - 16, x - 24, y - 1, 12, outline);
    drawThickSegment(hdc, x - 28, y - 16, x - 24, y - 1, 8, fur);
    fillCircle(hdc, x - 24, y - 1, 5, outline);
    fillCircle(hdc, x - 24, y - 1, 3, fur);

    drawThickSegment(hdc, x + 17, y - 42, x + 28 * dir, y - 26, 12, outline);
    drawThickSegment(hdc, x + 17, y - 42, x + 28 * dir, y - 26, 8, fur);
    drawThickSegment(hdc, x + 28 * dir, y - 26, x + 34 * dir, y - 8, 12, outline);
    drawThickSegment(hdc, x + 28 * dir, y - 26, x + 34 * dir, y - 8, 8, fur);
    fillCircle(hdc, x + 34 * dir, y - 8, 5, outline);
    fillCircle(hdc, x + 34 * dir, y - 8, 3, fur);

    drawThickSegment(hdc, x - 10, y - 14, x - 15, y + 8, 12, outline);
    drawThickSegment(hdc, x - 10, y - 14, x - 15, y + 8, 8, fur);
    drawThickSegment(hdc, x + 10, y - 14, x + 15, y + 8, 12, outline);
    drawThickSegment(hdc, x + 10, y - 14, x + 15, y + 8, 8, fur);
    fillEllipseColor(hdc, x - 21, y + 4, x - 7, y + 12, fur, outline, 1);
    fillEllipseColor(hdc, x + 7, y + 4, x + 21, y + 12, fur, outline, 1);
}

void paintScene(HDC hdc) {
    RECT fullRect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    HBRUSH skyBrush = CreateSolidBrush(RGB(22, 41, 188));
    FillRect(hdc, &fullRect, skyBrush);
    DeleteObject(skyBrush);
    drawSunAndBanana(hdc);

    for (const auto& building : game.buildings) {
        for (size_t i = 0; i < building.columnTops.size(); ++i) {
            int left = building.rect.left + static_cast<int>(i) * COLUMN_WIDTH;
            int right = left + COLUMN_WIDTH;
            fillRectColor(hdc, left, building.columnTops[i], right, GROUND_Y, building.color);

            for (int wy = building.columnTops[i] + 10; wy < GROUND_Y - 12; wy += 14) {
                if (wy + 6 >= GROUND_Y) {
                    break;
                }
                int blinkPhase = (game.animationTick / 10 + building.glowOffset + static_cast<int>(i) + wy / 14) % 6;
                bool lit = blinkPhase != 0 && blinkPhase != 4;
                COLORREF winColor = lit ? RGB(255, 242, 70) : RGB(96, 78, 68);
                if (lit) {
                    fillRectColor(hdc, left + 1, wy - 1, right - 1, wy + 7, RGB(255, 206, 48));
                }
                fillRectColor(hdc, left + 2, wy, right - 2, wy + 6, winColor);
            }
        }
    }

    drawPlayer(hdc, game.players[0]);
    drawPlayer(hdc, game.players[1]);

    if (game.projectile.active) {
        drawBananaShape(
            hdc,
            static_cast<int>(game.projectile.x),
            static_cast<int>(game.projectile.y),
            0.35,
            game.projectile.vx < 0.0
        );
    }

    if (game.explosion.active) {
        fillCircle(hdc, game.explosion.x, game.explosion.y, game.explosion.radius, RGB(255, 220, 80));
        fillCircle(hdc, game.explosion.x, game.explosion.y, std::max(2, game.explosion.radius / 2), RGB(255, 120, 40));
    }

    drawTextLine(hdc, 8, 8, "Player 1", 24, RGB(255, 255, 255), FW_BOLD);
    drawTextLine(hdc, 8, 36, ("Score:" + std::to_string(game.scores[0])), 24, RGB(255, 255, 255), FW_BOLD);
    drawTextLine(hdc, 8, 64, ("Angle:" + std::to_string(game.currentPlayer == 0 ? game.angle : 0)), 22, RGB(255, 255, 255));
    drawTextLine(hdc, 8, 90, ("Power:" + std::to_string(game.currentPlayer == 0 ? game.power : 0)), 22, RGB(255, 255, 255));

    std::string p2 = "Player 2";
    drawTextLine(hdc, WINDOW_WIDTH - 160, 8, p2, 24, RGB(255, 255, 255), FW_BOLD);
    drawTextLine(hdc, WINDOW_WIDTH - 160, 36, ("Score:" + std::to_string(game.scores[1])), 24, RGB(255, 255, 255), FW_BOLD);
    drawTextLine(hdc, WINDOW_WIDTH - 160, 64, ("Angle:" + std::to_string(game.currentPlayer == 1 ? game.angle : 0)), 22, RGB(255, 255, 255));
    drawTextLine(hdc, WINDOW_WIDTH - 160, 90, ("Power:" + std::to_string(game.currentPlayer == 1 ? game.power : 0)), 22, RGB(255, 255, 255));

    drawTextLine(hdc, WINDOW_WIDTH / 2 - 295, WINDOW_HEIGHT - 74, game.status, 18, RGB(255, 230, 180), FW_BOLD);
    drawTextLine(hdc, 12, WINDOW_HEIGHT - 48, "Controles: Arriba/Abajo = angulo   Izquierda/Derecha = potencia", 18, RGB(240, 240, 240));
   
drawTextLine(
    hdc,
    WINDOW_WIDTH / 2 - 80,
    40,
    "Viento: " + std::to_string(game.wind),
    22,
    RGB(255, 255, 255)
);
    drawTextLine(hdc, 12, WINDOW_HEIGHT - 24, "Acciones: Espacio = disparar   N = nueva ronda   R = reiniciar partida", 18, RGB(240, 240, 240));
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            startNewMatch();
            SetTimer(hwnd, 1, 16, nullptr);
            return 0;

        case WM_TIMER:
            game.animationTick = (game.animationTick + 1) % 6000;
            updateSimulation();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_KEYDOWN:
            switch (wParam) {
                case VK_UP:
                    if (!game.projectile.active) game.angle = std::min(85, game.angle + 1);
                    break;
                case VK_DOWN:
                    if (!game.projectile.active) game.angle = std::max(5, game.angle - 1);
                    break;
                case VK_RIGHT:
                    if (!game.projectile.active) game.power = std::min(120, game.power + 1);
                    break;
                case VK_LEFT:
                    if (!game.projectile.active) game.power = std::max(10, game.power - 1);
                    break;
                case VK_SPACE:
                    launchProjectile();
                    break;
                case 'N':
                    resetRound();
                    break;
                case 'R':
                    startNewMatch();
                    break;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
            HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDC, memBitmap));

            paintScene(memDC);
            BitBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const char CLASS_NAME[] = "GorilaBasWindow";

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    RegisterClassA(&wc);

    RECT windowRect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExA(
        0,
        CLASS_NAME,
        "GORILA.CPP - Juego en tiempo real",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (hwnd == nullptr) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
