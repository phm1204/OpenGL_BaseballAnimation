#include <GL/freeglut.h>
#include <cmath>
#include <string>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <random>

float dt = 0.016f;
const float FENCE_RADIUS = 60.0f;
const float WARNING_TRACK = 55.0f;

const float FIRST_BASE_TIME = 3.5f;
const float SECOND_BASE_TIME = 6.5f;
const float THIRD_BASE_TIME = 9.0f;

int score = 0;
int strikes = 0;
int balls = 0;
int outs = 0;

float batterProgress = 0.0f;

float runner1Progress = 0.0f;
float runner2Progress = 0.0f;
float runner3Progress = 0.0f;

bool swing = false;
bool homeRun = false;
bool groundBall = false;

float runnerTime = 0.0f;
bool runnerSafe = false;

bool runnerOn1st = false;
bool runnerOn2nd = false;
bool runnerOn3rd = false;

bool ballPickedUp = false;

float swingAngle = 0;
float fieldingDelay = 0.35f;
float landX = 0;
float landZ = 0;


enum State
{
    PITCHING,
    SWING,
    HIT,
    FOLLOW,
    GAMEOVER
};
State state = PITCHING;

struct Ball
{
    float x = 0;
    float y = 1;
    float z = 10;

    float vx = 0;
    float vy = 0;
    float vz = -3;

    bool hit = false;

    void update(float d)
    {
        if (hit)
        {
            vy -= 9.8f * d;
            if (y <= 0) {
                y = 0;
                if (groundBall){
                    vy = 0;
                    vx *= 0.985f;
                    vz *= 0.985f;
                } else {
                    vy = -vy * 0.25f;
                    vx *= 0.9f;
                    vz *= 0.9f;
                }
            }
        }

        x += vx * d;
        y += vy * d;
        z += vz * d;
    }
};
Ball ball;

struct Runner
{
    int base;
};
std::vector<Runner> runners;
std::default_random_engine eng((unsigned)time(NULL));

struct Fielder
{
    float x;
    float z;
    float homeX;
    float homeZ;
    float range;
    float speed;
    bool hasBall = false;
};
Fielder* ballOwner = nullptr;

struct ThrowBall
{
    bool active = false;

    float x;
    float y;
    float z;

    float targetX;
    float targetY;
    float targetZ;

    int targetBase;
};
ThrowBall throwBall;

Fielder pitcher = { 0, 10, 0, 10, 6, 3 };
Fielder firstMan = { 10,12, 10,12, 12,4 };
Fielder secondMan = { 6,18, 6,18, 14, 4 };
Fielder shortStop = { -6,18, -6,18, 14,4 };
Fielder thirdMan = { -10,12, -10,12, 12,4 };
Fielder leftField = { -18,40, -18,40, 28,4.5f };
Fielder center = { 0,48, 0,48, 30,4.5f };
Fielder rightField = { 18,40, 18,40, 28,4.5f };

void scoreRun() { 
    score++; 
}

void resetFielders()
{
    Fielder* fs[] =
    {
        &pitcher,
        &firstMan,
        &secondMan,
        &shortStop,
        &thirdMan,
        &leftField,
        &center,
        &rightField
    };

    for (int i = 0; i < 8; i++)
    {
        fs[i]->x = fs[i]->homeX;
        fs[i]->z = fs[i]->homeZ;
    }
}

void resetBall()
{
    ball.x = 0;
    ball.y = 1;
    ball.z = 10;
    ball.vx = 0;
    ball.vy = 0;
    ball.vz = -(3 + rand() % 4);

    ball.hit = false;
    swing = false;
    homeRun = false;

    ballPickedUp = false;
    ballOwner = nullptr;

    runnerTime = 0.0f;

    swingAngle = 0;
    fieldingDelay = 0.35f;

    resetFielders();
}

void resetGame()
{
    score = 0;
    strikes = 0;
    outs = 0;

    runners.clear();
    state = PITCHING;

    resetBall();
}

float distance2D(float x1, float z1, float x2, float z2) {
    float dx = x1 - x2; float dz = z1 - z2; return sqrt(dx * dx + dz * dz);
}

void advanceRunners(int bases)
{
    for (auto& r : runners) r.base += bases;
    for (int i = 0; i < (int)runners.size();)
    {
        if (runners[i].base >= 4)
        {
            scoreRun();
            runners.erase(runners.begin() + i);
        } else i++;
    }
    if (bases < 4)
    {
        Runner batter;
        batter.base = bases;
        runners.push_back(batter);
    }
}

void tripleHit() {
    advanceRunners(3);
}

void doubleHit() {
    advanceRunners(2);
}

void singleHit() {
    advanceRunners(1);
}

void homeRunHit() {
    score += (int)runners.size() + 1;
    runners.clear();
}

float nearestOutfielderDistance(float x, float z) {
    float best = 99999.0f; Fielder* fs[] = { &leftField, &center, &rightField }; for (auto f : fs) {
        float d = distance2D(x, z, f->x, f->z);
        best = std::min(best, d);
    }
    return best;
}

bool runnerOnFirst()
{
    for (auto& r : runners)
    {
        if (r.base == 1)
            return true;
    }
    return false;
}

bool checkHit()
{
    float dx = ball.x - 0.5f;
    float dy = ball.y - 1.5f;
    float dz = ball.z;

    return sqrt(dx * dx + dy * dy + dz * dz) < 1.5f;

} 

void updateFielder(Fielder& f)
{
    if (!ball.hit)
        return;

    float targetX = f.homeX;
    float targetZ = f.homeZ;

    float hitDist = sqrt(landX * landX + landZ * landZ);

    bool isOutfielder =
        (&f == &leftField) ||
        (&f == &center) ||
        (&f == &rightField);

    if (isOutfielder)
    {
        bool shouldMove = false;

        if (&f == &leftField)
        {
            if (landX < -5)
                shouldMove = true;
        }
        else if (&f == &center)
        {
            if (landX >= -15 && landX <= 15)
                shouldMove = true;
        }
        else if (&f == &rightField)
        {
            if (landX > 5)
                shouldMove = true;
        }

        if (shouldMove)
        {
            if (groundBall)
            {
                targetX = ball.x;
                targetZ = ball.z;
            }
            else
            {
                targetX = landX;
                targetZ = landZ;
            }
        }
    }
    else
    {
        if (hitDist < 40)
        {
            bool shouldMove = false;

            if (&f == &firstMan)
            {
                if (landX > 3)
                    shouldMove = true;
            }
            else if (&f == &secondMan)
            {
                if (landX > -3)
                    shouldMove = true;
            }
            else if (&f == &shortStop)
            {
                if (landX < 3)
                    shouldMove = true;
            }
            else if (&f == &thirdMan)
            {
                if (landX < -3)
                    shouldMove = true;
            }

            if (shouldMove)
            {
                targetX = ball.x;
                targetZ = ball.z;
            }
        }
    }

    float dx = targetX - f.x;
    float dz = targetZ - f.z;

    float dist = sqrt(dx * dx + dz * dz);

    if (dist < 0.1f)
        return;

    f.x += dx / dist * f.speed * dt;
    f.z += dz / dist * f.speed * dt;

    float fromCenter = sqrt(f.x * f.x + f.z * f.z);

    if (fromCenter > FENCE_RADIUS - 1)
    {
        float scale =
            (FENCE_RADIUS - 1) / fromCenter;

        f.x *= scale;
        f.z *= scale;
    }
}

bool catchBall(Fielder& f)
{
    if (ball.y > 2.0f)
        return false;

    float d =
        distance2D(
            f.x,
            f.z,
            ball.x,
            ball.z);

    float successRange = 2.0f;

    if (&f == &leftField ||
        &f == &center ||
        &f == &rightField)
    {
        successRange = 3.0f;
    }

    return d < successRange;
}

void processDoublePlay()
{
    bool doublePlay = groundBall && runnerOnFirst();
    if (doublePlay) {
        outs += 2;

        for (int i = 0; i < (int)runners.size(); i++) {
            if (runners[i].base == 1){
                runners.erase(runners.begin() + i);
                break;
            }
        }
    }
    else outs++;
    if (outs > 3)outs = 3;
}

bool pickupBall(Fielder& f)
{
    if (ball.y > 1.5f)
        return false;

    float d = distance2D(f.x, f.z, ball.x, ball.z);

    return d < 2.2f;
}

bool anyFielderCatch()
{
    Fielder* fs[] =
    {
        &firstMan,
        &secondMan,
        &shortStop,
        &thirdMan,
        &leftField,
        &center,
        &rightField
    };

    for (int i = 0; i < 7; i++)
    {
        if (pickupBall(*fs[i]))
        {
            ballPickedUp = true;
            ballOwner = fs[i];
            return true;
        }
    }

    return false;
}

bool checkHomeRun()
{
    float dist = sqrt(ball.x * ball.x + ball.z * ball.z);

    if (dist >= FENCE_RADIUS && ball.y > 1.0f) return true; 
    return false;
} 

void processHitResult() {
    float hitDist = sqrt(landX * landX + landZ * landZ);
    float gap = nearestOutfielderDistance(landX, landZ);

    if (hitDist >= FENCE_RADIUS)
    {
        homeRun = true;
        homeRunHit();
        return;
    }
    if (hitDist > 45 && gap > 8) {
        tripleHit();
        return;
    }
    if (hitDist > 25)
    {
        doubleHit();
        return;
    }
    singleHit();
}

void processStrike()
{
    strikes++;
    // 3 스트라이크 → 아웃
    if (strikes >= 3)
    {
        strikes = 0;
        balls = 0;
        outs++;
        // 3 아웃 → 게임 종료
        if (outs >= 3)
        {
            state = GAMEOVER;
        }
    }
}

void processBall()
{
    balls++;
    // 4 볼 → 볼넷 (주자 1루 진루)
    if (balls >= 4)
    {
        advanceRunners(1);
        balls = 0;
        strikes = 0;
    }
}

void updateThrowBall()
{
    if (!throwBall.active)
        return;

    float dx = throwBall.targetX - throwBall.x;
    float dy = throwBall.targetY - throwBall.y;
    float dz = throwBall.targetZ - throwBall.z;

    float d = sqrt(dx * dx + dy * dy + dz * dz);

    if (d < 1.0f)
    {
        throwBall.active = false;
        return;
    }

    float speed = 1.2f;

    throwBall.x += dx / d * speed;
    throwBall.y += dy / d * speed;
    throwBall.z += dz / d * speed;
}

void update()
{
    ball.update(dt);

    if (ball.hit)
        fieldingDelay += dt;

    updateFielder(firstMan);
    updateFielder(secondMan);
    updateFielder(shortStop);
    updateFielder(thirdMan);
    updateFielder(leftField);
    updateFielder(center);
    updateFielder(rightField);

    if (ball.hit)
    {
        if (checkHomeRun()) {
            homeRun = true;
            homeRunHit();
            resetBall();
            state = PITCHING;
            glutPostRedisplay();
            return;
        }
    }

    switch (state) {

    case PITCHING:
        if (ball.z < -2) {
            processStrike();

            if (outs >= 3) state = GAMEOVER;
            else resetBall();
        }
        if (swing) state = SWING;
        break;

    case SWING:
        swingAngle += 400 * dt;

        if (checkHit()) state = HIT;
        else if (ball.z < -2) 
        {
            if (swing) processStrike();
            else processBall();

            if (outs >= 3) state = GAMEOVER;
            else {
                resetBall();
                state = PITCHING;
            }
        }

        if (swingAngle > 120) {
            swing = false;
            swingAngle = 0;
            state = PITCHING;
        }
        break;

    case HIT:
    {
        runnerTime = 0.0f;
        ball.hit = true;

        float timing = fabs(ball.z);
        float speed = 16;
        float angle = 20;

        if (timing < 0.3f)
        {
            speed = 24;
            angle = 48;
        }
        else if (timing < 0.6f)
        {
            speed = 20;
            angle = 40;
        }

        std::uniform_real_distribution<float> spray(-4.0f, 4.0f);
        std::uniform_real_distribution<float> powerVar(0.85f, 1.15f);
        std::uniform_real_distribution<float> angleVar(-6.0f, 6.0f);

        int type = rand() % 4;

        if (type == 0)
        {
            angle += 12;
            speed *= 1.05f;
        }
        else if (type == 2)
        {
            angle = 5;
            speed *= 1.1f;
        }
        else if (type == 3)
        {
            angle = 8;
            speed *= 0.9f;
        }

        speed *= powerVar(eng);

        if (type != 2 && type != 3)
            angle += angleVar(eng);

        float rad = angle * 3.141592f / 180.0f;
        float side = spray(eng);

        ball.vx = (ball.x - 0.5f) * 6 + side * 5.0f;
        ball.vy = speed * sin(rad);
        ball.vz = speed * cos(rad);

        groundBall = (angle < 15);

        if (groundBall)
        {
            ball.vy *= 0.05f;
            ball.vx *= 1.4f;
            ball.vz *= 1.4f;
        }

        float t = (2.0f * ball.vy) / 9.8f;

        std::uniform_real_distribution<float> landingNoise(-2.5f, 2.5f);

        landX = ball.x + ball.vx * t + landingNoise(eng);
        landZ = ball.z + ball.vz * t + landingNoise(eng);

        state = FOLLOW;
        break;
    }

    case FOLLOW:
    {
        runnerTime += dt;

        if (anyFielderCatch())
        {
            bool infield =
                ballOwner == &firstMan ||
                ballOwner == &secondMan ||
                ballOwner == &shortStop ||
                ballOwner == &thirdMan;

            float throwTime;

            if (ballOwner == &firstMan)
                throwTime = 0.3f;
            else if (ballOwner == &secondMan)
                throwTime = 0.5f;
            else if (ballOwner == &shortStop)
                throwTime = 0.7f;
            else if (ballOwner == &thirdMan)
                throwTime = 0.9f;
            else
                throwTime = 1.6f;

            float totalTime = runnerTime + throwTime;

            if (totalTime < FIRST_BASE_TIME)
            {
                if (infield &&
                    groundBall &&
                    runnerOnFirst() &&
                    outs < 2)
                {
                    outs += 2;
                }
                else
                {
                    outs++;
                }
            }
            else
            {
                if (totalTime < SECOND_BASE_TIME)
                    singleHit();
                else if (totalTime < THIRD_BASE_TIME)
                    doubleHit();
                else
                    tripleHit();
            }

            if (outs > 3)
                outs = 3;

            if (outs >= 3)
                state = GAMEOVER;
            else
            {
                resetBall();
                state = PITCHING;
            }
        }

        break;
    }
    case GAMEOVER:
        break;
    }

    glutPostRedisplay();
}

void drawThrowBall()
{
    if (!throwBall.active)
        return;

    glColor3f(1, 1, 1);

    glPushMatrix();

    glTranslatef(
        throwBall.x,
        throwBall.y,
        throwBall.z);

    glutSolidSphere(0.25, 10, 10);

    glPopMatrix();
}

void drawBase(float x, float z)
{
    glPushMatrix();
    glColor3f(1, 1, 1);

    glTranslatef(x, 0.05f, z);
    glScalef(0.8f, 0.1f, 0.8f);

    glutSolidCube(1);
    glPopMatrix();
}

void drawField()
{
    glColor3f(0.15f, 0.60f, 0.15f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0, 0, 0);
    for (int a = -45; a <= 45; a++)
    {
        float rad = a * 3.141592f / 180.0f;
        glVertex3f(sin(rad) * 70, 0, cos(rad) * 70);
    } glEnd();

    glColor3f(0.65f, 0.45f, 0.25f);
    glBegin(GL_TRIANGLE_STRIP);

    for (int a = -45; a <= 45; a++)
    {
        float rad = a * 3.141592f / 180.0f;
        glVertex3f(sin(rad) * WARNING_TRACK, 0.01f, cos(rad) * WARNING_TRACK);
        glVertex3f(sin(rad) * FENCE_RADIUS, 0.01f, cos(rad) * FENCE_RADIUS);
    }
    glEnd();
    glColor3f(0.75f, 0.55f, 0.35f);
    glBegin(GL_QUADS);

    glVertex3f(0, 0.01f, 0);
    glVertex3f(14, 0.01f, 14);
    glVertex3f(0, 0.01f, 28);
    glVertex3f(-14, 0.01f, 14);

    glEnd();

    glColor3f(1, 1, 1);
    glBegin(GL_POLYGON);

    glVertex3f(-0.8f, 0.02f, 0);
    glVertex3f(-0.5f, 0.02f, -0.8f);
    glVertex3f(0.5f, 0.02f, -0.8f);
    glVertex3f(0.8f, 0.02f, 0);
    glVertex3f(0, 0.02f, 0.8f);

    glEnd();

    glPushMatrix();
    glTranslatef(0, 0.1f, 10);
    glColor3f(0.8f, 0.7f, 0.5f);
    glutSolidSphere(1.0, 20, 20);

    glPopMatrix();
    drawBase(13, 13);
    drawBase(0, 26);
    drawBase(-13, 13);
    glColor3f(1, 1, 1);

    glLineWidth(3);
    glBegin(GL_LINES);

    glVertex3f(0, 0.02f, 0);
    glVertex3f(45, 0.02f, 45);
    glVertex3f(0, 0.02f, 0);
    glVertex3f(-45, 0.02f, 45);

    glEnd();

    glColor3f(0.2f, 0.2f, 0.8f);
    glBegin(GL_LINE_STRIP);

    for (int a = -45; a <= 45; a++)
    {
        float rad = a * 3.141592f / 180.0f;
        glVertex3f(sin(rad) * FENCE_RADIUS, 3, cos(rad) * FENCE_RADIUS);
    }
    glEnd();
}

void setCamera()
{
    if (state == FOLLOW)
        gluLookAt(ball.x + 5, ball.y + 6, ball.z - 15, ball.x, ball.y, ball.z, 0, 1, 0);
    
    else gluLookAt(0, 12, -25, 0, 2, 15, 0, 1, 0);
    
}

void init()
{
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.5f, 0.8f, 1.0f, 1.0f);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(60.0, 800.0 / 600.0, 0.1, 200.0);
    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);

    GLfloat lightPos[] = { 30.0f, 80.0f, -20.0f, 1.0f };
    GLfloat diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);

    glShadeModel(GL_SMOOTH);
}

void drawPlayer(float x, float z, float r, float g, float b) {
    glPushMatrix();

    glDisable(GL_LIGHTING);
    glColor4f(0, 0, 0, 0.35f);
    glTranslatef(x, 0.01f, z);
    glScalef(1.2f, 0.05f, 1.2f);
    glutSolidSphere(0.5, 16, 16);

    glEnable(GL_LIGHTING);

    glPopMatrix();

    glPushMatrix();
    glTranslatef(x, 1.0f, z);
    glColor3f(r, g, b);
    glEnable(GL_NORMALIZE);

    glutSolidCube(1);
    glColor3f(1.0f, 0.8f, 0.6f);
    glTranslatef(0, 0.8f, 0);
    glutSolidSphere(0.25, 20, 20);
    glPopMatrix();
}

void drawFielders()
{
    drawPlayer(pitcher.x, pitcher.z, 0, 0, 1);
    drawPlayer(firstMan.x, firstMan.z, 0, 0, 1);
    drawPlayer(secondMan.x, secondMan.z, 0, 0, 1);
    drawPlayer(shortStop.x, shortStop.z, 0, 0, 1);
    drawPlayer(thirdMan.x, thirdMan.z, 0, 0, 1);
    drawPlayer(leftField.x, leftField.z, 0, 0, 1);
    drawPlayer(center.x, center.z, 0, 0, 1);
    drawPlayer(rightField.x, rightField.z, 0, 0, 1);
}

void drawBatter()
{
    drawPlayer(0, 0, 1, 1, 1);
    glPushMatrix();
    glTranslatef(0.8f, 1.7f, 0.2f);
    glRotatef(swingAngle, 0.7f, 0.0f, 0.7f);

    float swingScale = 1.0f;
    if (state == SWING)
    {
        if (swingAngle < 25) swingScale = 1.05f;
        else if (swingAngle < 60) swingScale = 1.25f;
        else swingScale = 0.95f;
        
    }
    glTranslatef(0, 0.7f, 0);

    if (state == SWING && swingAngle > 20 && swingAngle < 60)
        glColor3f(0.9f, 0.7f, 0.2f);
    
    else glColor3f(0.6f, 0.3f, 0.1f);
    
    glScalef(0.12f * swingScale, 2.2f * swingScale, 0.12f * swingScale);
    glutSolidCube(1);
    glPopMatrix();

    if (state == HIT || state == FOLLOW)
    {
        glPushMatrix();
        float flash = 0.3f;

        glColor4f(1.0f, 1.0f, 0.5f, flash);
        glTranslatef(0.5f, 1.5f, 0);
        glScalef(2.0f, 0.2f, 2.0f);
        glutSolidSphere(0.4, 10, 10);

        glPopMatrix();
    }

    if (state == HIT)
    {
        glPushMatrix();
        glColor3f(1, 0.9f, 0.2f);
        glTranslatef(0.5f, 1.5f, 0.2f);
        glutSolidSphere(0.15, 10, 10);
        glPopMatrix();
    }
}

void drawBall()
{
    glPushMatrix();

    if (ball.hit) glColor3f(1, 1, 0);
    else glColor3f(1, 0, 0);

    glTranslatef(ball.x, ball.y, ball.z);
    glutSolidSphere(0.2, 20, 20);
    glPopMatrix();
}

void drawText(float x, float y, const std::string& s) 
{
    glRasterPos2f(x, y);

    for (size_t i = 0; i < s.size(); i++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, s[i]);
    
}

void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    setCamera();

    drawField();
    drawFielders();
    drawBatter();
    drawBall();

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    gluOrtho2D(0, 800, 0, 600);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3f(1, 1, 1);
    drawText(20, 560, "Score : " + std::to_string(score));
    drawText(20, 530, "Strike : " + std::to_string(strikes));
    drawText(20, 500, "Out : " + std::to_string(outs));

    int b1 = 0;
    int b2 = 0;
    int b3 = 0;

    for (size_t i = 0; i < runners.size(); i++)
    {
        if (runners[i].base == 1) b1++;
        if (runners[i].base == 2) b2++;
        if (runners[i].base == 3) b3++;
    }

    drawText(20, 470, "1B : " + std::to_string(b1));
    drawText(20, 440, "2B : " + std::to_string(b2));
    drawText(20, 410, "3B : " + std::to_string(b3));

    if (homeRun)drawText(320, 300, "HOME RUN!");
    if (state == GAMEOVER) 
        drawText(250, 260, "GAME OVER (R TO RESTART)");

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glutSwapBuffers();
}

void keyboard(unsigned char key, int, int) 
{
    if (key == ' ' && state == PITCHING) swing = true;
    if (key == 'r' || key == 'R') resetGame();
    if (key == 27) exit(0);
}

void timer(int)
{
    update();
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);

    glutCreateWindow("Mini Baseball Complete Edition");

    init();
    resetGame();

    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(16, timer, 0);
    glutMainLoop();

    return 0;
}