#include <cstring>

#include "playerorder.h"

namespace World
{

template <typename T>
static void Put(std::string &out, T v)
{
    out.append((const char *)&v, sizeof(T));
}

template <typename T>
static bool Take(const uint8_t *&p, const uint8_t *end, T *out)
{
    if ( p + sizeof(T) > end )
        return false;

    memcpy(out, p, sizeof(T));
    p += sizeof(T);
    return true;
}

std::string PlayerOrder::Write() const
{
    std::string out(1, (char)Type);
    Put(out, Action);
    Put(out, Proto);
    Put(out, Unit);
    Put(out, Target);
    Put(out, Sector);
    Put(out, (float)Point.x);
    Put(out, (float)Point.y);
    Put(out, (float)Point.z);
    Put(out, Value);
    Put(out, Energy);
    return out;
}

bool PlayerOrder::Read(const uint8_t *data, size_t size)
{
    const uint8_t *p = data;
    const uint8_t *end = data + size;
    uint8_t type;
    float x, y, z;

    if ( !Take(p, end, &type) || !Take(p, end, &Action) || !Take(p, end, &Proto) || !Take(p, end, &Unit) ||
         !Take(p, end, &Target) || !Take(p, end, &Sector) || !Take(p, end, &x) || !Take(p, end, &y) ||
         !Take(p, end, &z) || !Take(p, end, &Value) || !Take(p, end, &Energy) || p != end )
        return false;

    switch ( type )
    {
    case COMMAND:
    case AGGRESSION:
    case REORGANIZE:
    case GET_IN:
    case FILL_MODE:
        break;

    default:
        return false;
    }

    Type = (Kind)type;
    Point = vec3d(x, y, z);
    return true;
}

}
