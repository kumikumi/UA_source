#ifndef WORLD_NPARTICLE_H_INCLUDED
#define WORLD_NPARTICLE_H_INCLUDED

#include <deque>
#include "../system/tform.h"
#include "../system/gfx.h"
#include "../vectors.h"

class NC_STACK_particle;
class NC_STACK_skeleton;
namespace UAskeleton { struct Data; };
struct TObjectRenderParams;

namespace World
{

class ParticleSystem
{
public:
    
protected:
    struct Frak
    {
        NC_STACK_particle *pParticleGen = NULL;
        
        vec3d Pos;
        vec3d Vec;
        
        int32_t Age = 0;
        
        Frak(NC_STACK_particle *base, const vec3d& pos, const vec3d& vec, int32_t age = 0)
        : pParticleGen(base), Pos(pos), Vec(vec), Age(age) 
        {};
    };
    
public:
    ParticleSystem();
    
    void AddParticle(NC_STACK_particle *base, const vec3d& pos, const vec3d& vec, int32_t age = 0);
    
    void UpdateRender(TObjectRenderParams *rndrParams, int32_t delta);
    
    void Clear();
    
protected:
    void Render(Frak *p, float scale, TObjectRenderParams *rndrParams);
    
    
protected:
    std::deque<Frak> _particles;
    
    bool _disableAdd = false;
};
    
    
}

#endif