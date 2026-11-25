#ifndef YFLYER_H_INCLUDED
#define YFLYER_H_INCLUDED

#include "nucleas.h"
#include "ypabact.h"

class NC_STACK_ypaflyer: public NC_STACK_ypabact
{
public:
    virtual size_t Init(IDVList &stak);
    virtual size_t Deinit();
    virtual size_t SetParameters(IDVList &stak) override;
    virtual void AI_layer3(update_msg *arg);
    virtual void User_layer(update_msg *arg);
    virtual void Move(move_msg *arg);
    virtual size_t SetPosition(TUnitPlacement *arg);
    virtual void Renew();
    virtual void VisitState(World::State::Visitor &v) override;
    virtual void HandBrake(update_msg *arg);

    NC_STACK_ypaflyer() 
    {
        _flyerBoost = 0.0;
        _flyerType    = 0;
    };
    virtual ~NC_STACK_ypaflyer() {};
    
    virtual const std::string ClassName() const {
        return __ClassName;
    };

    enum FLY_ATT
    {
        FLY_ATT_TYPE = 0x80002000
    };

    virtual void setFLY_type(int);

    virtual int getFLY_type();
    
    
    void PitchAndBank(float dtime, vec3d v, int isUser);
    void TurnToTargetAndSetLift(float angl);

    //Data
    static constexpr const char * __ClassName = "ypaflyer.class";
    
    float _flyerBoost = 0.0;
    int _flyerType    = 0;
};

#endif // YFLYER_H_INCLUDED
