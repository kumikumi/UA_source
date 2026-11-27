#include <cstring>

#include "viewstream.h"
#include "../log.h"

namespace World
{

static void Put16(std::string &out, uint16_t v)
{
    out.append((const char *)&v, 2);
}

static void Put32(std::string &out, uint32_t v)
{
    out.append((const char *)&v, 4);
}

static void PutBytes(std::string &out, const std::string &bytes)
{
    Put32(out, bytes.size());
    out += bytes;
}

std::string ViewStream::Hello(int level, const std::string &description, const State::Schema &schema)
{
    std::string msg(1, (char)HELLO);
    Put16(msg, Format);
    Put32(msg, (uint32_t)level);
    PutBytes(msg, description);
    PutBytes(msg, State::WriteSchema(schema));
    return msg;
}

std::string ViewStream::Image(const Recorder &recorder)
{
    std::string msg(1, (char)IMAGE);
    Put32(msg, recorder.Step());
    recorder.WriteViewImage(msg);
    return msg;
}

std::string ViewStream::Step(const Recorder &recorder)
{
    std::string msg(1, (char)STEP);
    Put32(msg, recorder.Step());
    msg.push_back(recorder.GuessesStartedOver() ? 1 : 0);
    msg += recorder.LastStepData();
    return msg;
}

std::string ViewStream::You(uint32_t player)
{
    std::string msg(1, (char)YOU);
    Put32(msg, player);
    return msg;
}

std::string ViewStream::End()
{
    return std::string(1, (char)END);
}


// Reading

uint8_t ViewStreamReader::Fail(const char *why)
{
    ypa_log_out("The view stream doesn't read: %s\n", why);
    _failed = true;
    return 0;
}

// Takes a little-endian value or bytes from where p is, if they're there
template <typename T>
static bool Take(const uint8_t *&p, const uint8_t *end, T *out)
{
    if ( p + sizeof(T) > end )
        return false;

    memcpy(out, p, sizeof(T));
    p += sizeof(T);
    return true;
}

static bool TakeBytes(const uint8_t *&p, const uint8_t *end, std::string *out)
{
    uint32_t size;

    if ( !Take(p, end, &size) || p + size > end )
        return false;

    out->assign((const char *)p, size);
    p += size;
    return true;
}

uint8_t ViewStreamReader::Read(const uint8_t *data, size_t size)
{
    if ( _failed )
        return 0;

    if ( !size )
        return Fail("an empty message");

    const uint8_t *p = data + 1;
    const uint8_t *end = data + size;

    switch ( data[0] )
    {
    case ViewStream::HELLO:
    {
        uint16_t format;
        uint32_t level;
        std::string schema;

        if ( !Take(p, end, &format) || format != ViewStream::Format )
            return Fail("another format");

        if ( !Take(p, end, &level) || !TakeBytes(p, end, &_description) || !TakeBytes(p, end, &schema) ||
             !State::ReadSchema(schema, _schema) || _schema.empty() || !MakeLayouts(_schema, _layouts) )
            return Fail("a hello without a level or a view's fields");

        _level = (int32_t)level;
        _hasLevel = true;
        _hasView = false;
        _view.clear();
        return ViewStream::HELLO;
    }

    case ViewStream::IMAGE:
    {
        uint32_t step, count;

        if ( !_hasLevel )
            return Fail("an image before the level");

        if ( !Take(p, end, &step) || !Take(p, end, &count) )
            return Fail("an image cut short");

        _view.clear();
        _changed.clear();

        for (uint32_t i = 0; i < count; i++)
        {
            uint8_t kind;
            uint32_t id;

            if ( !Take(p, end, &kind) || !Take(p, end, &id) )
                return Fail("an image cut short");

            auto it = _layouts.find(kind);
            if ( it == _layouts.end() )
                return Fail("an object of a kind without fields");

            size_t objSize = it->second.Offsets.back();
            if ( p + 2 * objSize > end )
                return Fail("an image cut short");

            ObjectVersions &obj = _view[(uint64_t)kind << 32 | id];
            obj.Data.assign((const char *)p, objSize);
            obj.Before.assign((const char *)p + objSize, objSize);
            p += 2 * objSize;
        }

        for ( const auto &it : _view )
            _changed.emplace_back(it.first, &it.second);

        _step = step;
        _hasView = true;
        return ViewStream::IMAGE;
    }

    case ViewStream::STEP:
    {
        uint32_t step;
        uint8_t startOver;

        // Steps before the image, or of a view taken anew before its image
        // comes, can't be read
        if ( !_hasView )
            return ViewStream::STEP;

        if ( !Take(p, end, &step) || !Take(p, end, &startOver) )
            return Fail("a step cut short");

        if ( step != _step + 1 )
            return Fail("a step missed");

        if ( startOver )
        {
            for ( auto &it : _view )
                it.second.Before = it.second.Data;
        }

        _changed.clear();

        if ( !ApplyViewChanges(_view, _layouts, (const char *)p, (const char *)end, &_changed) )
            return Fail("a step's changes");

        _step = step;
        return ViewStream::STEP;
    }

    case ViewStream::YOU:
        if ( !Take(p, end, &_player) )
            return Fail("a player cut short");

        return ViewStream::YOU;

    case ViewStream::END:
        _ended = true;
        return ViewStream::END;

    default:
        return Fail("a message of no type it knows");
    }
}

}
