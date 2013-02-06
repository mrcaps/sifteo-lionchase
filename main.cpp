/*
 * "Lion Chase" game for Sifteo Cubes
 * mrcaps, 2013
 */

#include <sifteo.h>
#include "assets.gen.h"
using namespace Sifteo;

static Metadata M = Metadata()
    .title("Sifteo test")
    .package("com.mrcaps.sifteo.walk", "0.1")
    .icon(Icon)
    .cubeRange(0, CUBE_ALLOCATION);

AssetSlot gMainSlot = AssetSlot::allocate()
    .bootstrap(BootstrapAssets);
static AssetLoader loader;
static AssetConfiguration<1> config;

static VideoBuffer vid[CUBE_ALLOCATION];

static unsigned const CUBE_PIXELS = 128;

Side connected[CUBE_ALLOCATION][NUM_SIDES];

Side opposite(Side in) {
    switch (in) {
    case RIGHT:
        return LEFT;
    case LEFT:
        return RIGHT;
    case BOTTOM:
        return TOP;
    case TOP:
        return BOTTOM;
    case NO_SIDE:
        return NO_SIDE;
    default:
        return NO_SIDE;
    }
}

class Beest {
public:
    Beest(unsigned my_spritedex, CubeID my_cube, Float2 my_x, Float2 my_targx) :
        spritedim(32),
        spritedx(my_spritedex),
        cube(my_cube),
        x(my_x),
        targx(my_targx)
    {

    }

    void update(TimeDelta tdelta) {
        x += (targx - x) * 0.01f;
    }

    void draw() {
        vid[cube].sprites[spritedx].setImage(Beasts[0]);
        vid[cube].sprites[spritedx].move(x - vec(spritedim, spritedim));        
    }

private:
    unsigned spritedim;
    unsigned spritedx;

    CubeID cube;
    Float2 x;
    Float2 targx;
};

class Actor {
public:
    Actor(CubeID startCube) : 
        cube(startCube), 
        spritedim(32/2),
        gameRunning(true),
        target(RIGHT),
        targetSet(true)
    {
        x.set(CUBE_PIXELS/2, CUBE_PIXELS/2);
        v.set(0, 0);

        targx.set(200, 64);
    }

    /// Move the actor to a new side if one exists.
    Side checkSide(Side s) {
        Neighborhood nb(cube);
        if (nb.hasNeighborAt(s)) {
            //old cube
            vid[cube].sprites[0].hide();
            //connected side
            Side newside = connected[cube][s];
            switch (newside) {
            case LEFT:
                x.set(0.0f - spritedim, CUBE_PIXELS/2);
                v.set(v.len(), 0.0f);
                break;
            case TOP:
                x.set(CUBE_PIXELS/2, 0.0f - spritedim);
                v.set(0.0f, v.len());
                break;
            case BOTTOM:
                x.set(CUBE_PIXELS/2, CUBE_PIXELS + spritedim);
                v.set(0.0f, -v.len());
                break;
            case RIGHT:
                x.set(CUBE_PIXELS - spritedim, CUBE_PIXELS/2);
                v.set(-v.len(), 0.0f);
                break;
            default:
                LOG("No connected side - badness!\n");
                break;
            }

            //new cube
            cube = nb.cubeAt(s);
            targetSet = false;
            //move towards the center of the display
            targx = vec(CUBE_PIXELS/2, CUBE_PIXELS/2);
            return newside;
        } else {
            return NO_SIDE;
        }
    }

    /// Get a new target side for our main actor
    void newTarget() {
        Side oldTarget = opposite(target);
        do {
            target = rnd.randrange(NUM_SIDES);
        } while (target == oldTarget);
        targx = vec(CUBE_PIXELS/2, CUBE_PIXELS/2) + 
            Float2::unit(target) * 100.0f;
        LOG("[actor] new target\n");
    }

    /// Update actor logic
    void update(TimeDelta tdelta) {
        if (!targetSet && 
            (x - vec(CUBE_PIXELS/2, CUBE_PIXELS/2)).len2() < 900.0f) 
        {
            newTarget();
            targetSet = true;
        }
        //acceleration is the last term
        v += (((targx - x).normalize() * 0.7f) - v) * float(tdelta) * 0.8f;
        x += v;

        //if we're close to the border, ensure there's another cube
        //TODO: do a 
        //  Events::neighborAdd.set
        Side s;
        if (x.x > CUBE_PIXELS + spritedim) {
            if ((s = checkSide(RIGHT)) == NO_SIDE) {
                gameRunning = false;
            }
        } else if (x.x < 0 - spritedim) {
            if ((s = checkSide(LEFT)) == NO_SIDE) {
                gameRunning = false;
            }
        } else if (x.y > CUBE_PIXELS + spritedim) {
            if ((s = checkSide(BOTTOM)) == NO_SIDE) {
                gameRunning = false;
            }
        } else if (x.y < 0 - spritedim) {
            if ((s = checkSide(TOP)) == NO_SIDE) {
                gameRunning = false;
            }
        }
    }

    /// Draw actor to appropriate cube
    void draw() {
        PinnedAssetImage todraw;
        if (abs(v.y) > abs(v.x)) {
            todraw = Faces[0]; //front face for up/down
        } else {
            if (v.x < 0) {
                todraw = Faces[1];
            } else {
                todraw = Faces[2];
            }
        }
        vid[cube].sprites[0].setImage(todraw);
        vid[cube].sprites[0].move(x - vec(spritedim, spritedim));
    }

    /// Are we still running the game?
    bool isRunning() {
        return gameRunning;
    }

private:
    Float2 x, v; //position, velocity
    CubeID cube; //current cube
    int spritedim; //radius of sprite
    bool gameRunning;

    Random rnd;
    Side target; //which side should we move to?
    Float2 targx;
    bool targetSet;
};

class Listener {
public:
    Listener() {
        LOG("[listener] constructing\n");
        Events::cubeConnect.set(&Listener::onConnect, this);
        Events::neighborAdd.set(&Listener::onNeighborAdd, this);
        Events::neighborRemove.set(&Listener::onNeighborRemove, this);

        for (CubeID cube : CubeSet::connected()) {
            onConnect(cube);
        }

        loader.start(config);
        while (!loader.isComplete()) {
            System::paint();
        }
        loader.finish();

        /* it seems this is allocated already?
        for (unsigned i = 0; i < CUBE_ALLOCATION; ++i) {
            connected[i] = new CubeID[NUM_SIDES];
        }*/

        LOG("[listener] done constructing\n");
    }

    ~Listener() {}

    void onConnect(unsigned id) {
        LOG("[listener] connected cube %d\n", id);

        CubeID cube(id);
        vid[id].initMode(BG0_SPR_BG1);
        vid[id].bg0.image(vec(0,0), Backgrounds, 0);
        vid[id].attach(cube);
    }

    void setBackground(unsigned dx) {
        for (CubeID cube : CubeSet::connected()) {
            vid[cube].bg0.image(vec(0,0), Backgrounds, dx);
        }
    }

    void onNeighborAdd(unsigned c0, unsigned s0, unsigned c1, unsigned s1) {
        LOG("[listener] neighborAdd %d[%d] to %d[%d]\n", c0, s0, c1, s1);

        connected[c0][s0] = Side(s1);
        connected[c1][s1] = Side(s0);
    }

    void onNeighborRemove(unsigned c0, unsigned s0, unsigned c1, unsigned s1) {
        LOG("[listener] neighborAdd %d[%d] to %d[%d]\n", c0, s0, c1, s1);

        connected[c0][s0] = NO_SIDE;
        connected[c1][s1] = NO_SIDE;
    }
};


void main() {
    config.append(gMainSlot, BootstrapAssets);
    loader.init();

    TimeStep ts;

    Listener listen;
    Actor actor(CubeID(0));

    while (1) {
        ts.next();

        if (actor.isRunning()) {
            actor.update(ts.delta());
            actor.draw();
        } else {
            //game over!
            listen.setBackground(2);
        }

        System::paint();
    }
}
