#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <zlib.h>

#include "recorder.h"
#include "../yw.h"
#include "../IFFile.h"
#include "../utils.h"
#include "../env.h"
#include "../log.h"

namespace World
{

// Little-endian values into a byte string, as the platforms that build this
// keep them, like the state's values

static void Put8(std::string &out, uint8_t v)
{
    out.push_back((char)v);
}

static void Put16(std::string &out, uint16_t v)
{
    out.append((const char *)&v, 2);
}

static void Put32(std::string &out, uint32_t v)
{
    out.append((const char *)&v, 4);
}

static void PutText(std::string &out, const std::string &text)
{
    size_t length = std::min<size_t>(text.size(), 0xFFFF);
    Put16(out, length);
    out.append(text, 0, length);
}

// A file of the game's folder by its name with a prefix (save:...), else a
// file by its path. NULL when it can't be opened.
static FSMgr::FileHandle *OpenFile(const std::string &path, const std::string &mode)
{
    if ( Common::Env.ApplyPrefix(path) != path )
        return uaOpenFileAlloc(path, mode);

    FSMgr::FileHandle *file = new FSMgr::FileHandle(path, mode);

    if ( !file->OK() )
    {
        delete file;
        return NULL;
    }

    return file;
}

static uint64_t ObjectKey(uint8_t kind, uint32_t id)
{
    return ((uint64_t)kind << 32) | id;
}

static std::string Platform()
{
    std::string platform;

#if defined(__APPLE__)
    platform = "macOS";
#elif defined(_WIN32)
    platform = "Windows";
#elif defined(__linux__)
    platform = "Linux";
#else
    platform = "unknown";
#endif

#if defined(__aarch64__) || defined(__arm64__)
    platform += " arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    platform += " x86-64";
#endif

    return platform;
}

static bool Unpacked(const std::string &packed, std::string &data)
{
    if ( packed.size() < 4 )
        return false;

    uint32_t size;
    memcpy(&size, packed.data(), 4);

    data.assign(size, '\0');
    uLongf outSize = size;

    return uncompress((Bytef *)&data[0], &outSize, (const Bytef *)packed.data() + 4, packed.size() - 4) == Z_OK &&
           outSize == size;
}


// A float field's value guessed from its last two: the last, moved on by as
// much as it last moved. The guess is what a change is XORed with, so the
// closer it is, the more of the XOR is zeros. Written so that no compiler
// fuses it into one rounding, and only with finite values, whose arithmetic
// every platform does alike.
template <class T>
static void PredictElements(const char *last, const char *before, char *guess, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        T v1, v0;
        memcpy(&v1, last + i * sizeof(T), sizeof(T));
        memcpy(&v0, before + i * sizeof(T), sizeof(T));

        T g = v1;
        if ( std::isfinite(v1) && std::isfinite(v0) )
        {
            T moved = v1 - v0;
            T next = v1 + moved;

            if ( std::isfinite(moved) && std::isfinite(next) )
                g = next;
        }

        memcpy(guess + i * sizeof(T), &g, sizeof(T));
    }
}

// What a field's new bytes are XORed with: the guess for floats, else the
// last bytes
static void Guess(const State::FieldInfo &field, const char *last, const char *before, size_t size, char *guess)
{
    if ( before && field.ElemType == State::T_F32 )
        PredictElements<float>(last, before, guess, size / 4);
    else if ( before && field.ElemType == State::T_F64 )
        PredictElements<double>(last, before, guess, size / 8);
    else
        memcpy(guess, last, size);
}

// The bytes of a field before the last change, when they fit
static const char *BeforeBytes(const State::FieldInfo &field, const std::string &before,
                               const std::vector<size_t> &beforeOffsets, bool beforeFits, size_t i, size_t size)
{
    if ( !beforeFits || field.List || beforeOffsets[i + 1] - beforeOffsets[i] != size )
        return NULL;

    return before.data() + beforeOffsets[i];
}

// What changed of an object: its fields that differ from the last bytes,
// written as the format says. beforeOffsets is NULL when the bytes before
// don't fit the fields.
static void PutChangedFields(std::string &out, const std::vector<State::FieldInfo> &fields, const ObjectVersions &old,
                             const std::vector<size_t> &oldOffsets, const std::string &newData,
                             const std::vector<size_t> &newOffsets, const std::vector<size_t> *beforeOffsets)
{
    size_t countAt = out.size();
    uint16_t count = 0;
    Put16(out, 0);

    char guess[256];

    for (size_t i = 0; i < fields.size(); i++)
    {
        size_t oldSize = oldOffsets[i + 1] - oldOffsets[i];
        size_t newSize = newOffsets[i + 1] - newOffsets[i];
        const char *o = old.Data.data() + oldOffsets[i];
        const char *n = newData.data() + newOffsets[i];

        if ( oldSize == newSize && !memcmp(o, n, newSize) )
            continue;

        Put16(out, i);

        if ( fields[i].List || newSize > sizeof(guess) )
        {
            out.append(n, newSize);
        }
        else
        {
            const char *before = NULL;
            if ( beforeOffsets && (*beforeOffsets)[i + 1] - (*beforeOffsets)[i] == newSize )
                before = old.Before.data() + (*beforeOffsets)[i];

            Guess(fields[i], o, before, newSize, guess);

            size_t at = out.size();
            out.resize(at + newSize);
            char *dst = &out[at];

            for (size_t b = 0; b < newSize; b++)
                dst[b] = guess[b] ^ n[b];
        }

        count++;
    }

    memcpy(&out[countAt], &count, 2);
}

// Applies what changed, as the format says, to objects by key. Returns where
// it ended, or NULL when it doesn't read.
static const char *ApplyChanges(std::map<uint64_t, ObjectVersions> &objects, const State::Schema &schema,
                                const char *p, const char *end)
{
    std::vector<size_t> offsets, beforeOffsets;
    char guess[256];

    if ( p + 4 > end )
        return NULL;

    uint32_t changes;
    memcpy(&changes, p, 4);
    p += 4;

    for (uint32_t c = 0; c < changes; c++)
    {
        if ( p + 6 > end )
            return NULL;

        uint8_t kind = p[0], what = p[5];
        uint32_t id;
        memcpy(&id, p + 1, 4);
        p += 6;

        uint64_t key = ObjectKey(kind, id);

        if ( what == 0 )
        {
            objects.erase(key);
        }
        else if ( what == 1 )
        {
            uint32_t size;
            if ( p + 4 > end )
                return NULL;

            memcpy(&size, p, 4);
            p += 4;

            if ( p + size > end )
                return NULL;

            ObjectVersions &obj = objects[key];
            obj.Data.assign(p, size);
            obj.Before = obj.Data;
            p += size;
        }
        else
        {
            auto sit = schema.find(kind);
            auto oit = objects.find(key);

            if ( sit == schema.end() || oit == objects.end() || p + 2 > end ||
                 !State::FieldOffsets(sit->second, oit->second.Data, offsets) )
                return NULL;

            const std::vector<State::FieldInfo> &fields = sit->second;
            const std::string &old = oit->second.Data;
            bool beforeFits = State::FieldOffsets(fields, oit->second.Before, beforeOffsets);

            uint16_t n;
            memcpy(&n, p, 2);
            p += 2;

            std::string updated;
            size_t field = 0;

            for (uint16_t i = 0; i < n; i++)
            {
                uint16_t index;
                if ( p + 2 > end )
                    return NULL;

                memcpy(&index, p, 2);
                p += 2;

                if ( index >= fields.size() || index < field )
                    return NULL;

                updated.append(old, offsets[field], offsets[index] - offsets[field]);

                size_t size = State::FieldSize(fields[index], p, end - p);
                if ( !size )
                    return NULL;

                if ( fields[index].List || size > sizeof(guess) )
                {
                    updated.append(p, size);
                }
                else
                {
                    if ( offsets[index + 1] - offsets[index] != size )
                        return NULL;

                    Guess(fields[index], old.data() + offsets[index],
                          BeforeBytes(fields[index], oit->second.Before, beforeOffsets, beforeFits, index, size), size, guess);

                    for (size_t b = 0; b < size; b++)
                        updated.push_back(guess[b] ^ p[b]);
                }

                p += size;
                field = index + 1;
            }

            updated.append(old, offsets[field], old.size() - offsets[field]);
            oit->second.Before = std::move(oit->second.Data);
            oit->second.Data = std::move(updated);
        }
    }

    return p;
}


// Recorder

Recorder::Recorder() {}
Recorder::~Recorder() {}

bool Recorder::Open(NC_STACK_ypaworld *world, const std::string &path, const std::string &startedFrom,
                    const State::Image &startView, const State::Image &startInternal)
{
    FSMgr::FileHandle *file = OpenFile(path, "wb");

    if ( !file )
    {
        ypa_log_out("Unable to write the recording %s\n", path.c_str());
        return false;
    }

    _path = path;
    _fileHandle = file;
    _file.reset(new IFFile(file, true));
    _file->pushChunk(TAG_RPLY, TAG_FORM, -1);

    std::string info;
    Put16(info, 2);
    Put16(info, startView.Level);

    for (const std::string &text : {startedFrom, Platform(),
#if defined(__VERSION__)
                                    std::string(__VERSION__),
#else
                                    std::string(),
#endif
                                    std::string(__DATE__ " " __TIME__)})
    {
        info += text;
        info.push_back('\0');
    }

    _viewSchema = State::MakeSchema(world, State::PART_VIEW);
    _internalSchema = State::MakeSchema(world, State::PART_INTERNAL);
    _viewLayouts = MakeLayouts(_viewSchema);
    _internalLayouts = MakeLayouts(_internalSchema);

    for (auto chunk : {std::make_pair(TAG_INFO, info),
                       std::make_pair(TAG_SCHV, State::WriteSchema(_viewSchema)),
                       std::make_pair(TAG_SCHI, State::WriteSchema(_internalSchema)),
                       std::make_pair(TAG_STRV, State::Pack(startView)),
                       std::make_pair(TAG_STRI, State::Pack(startInternal))})
    {
        _file->pushChunk(0, chunk.first, -1);
        _file->write(chunk.second.data(), chunk.second.size());
        _file->popChunk();
    }

    if ( _packer )
    {
        deflateEnd(_packer.get());
        _packer.reset();
    }

    _packed.clear();
    _blockSteps = 0;
    _messages.clear();
    _messageCount = 0;
    _hasPending = false;
    _lastView.clear();
    _lastInternal.clear();
    _step = 0;
    return true;
}

void Recorder::Remember(NC_STACK_ypaworld *world, State::Part part, LastObjects &last)
{
    last.clear();

    State::ForEachObject(world, part, [this, &last](uint8_t kind, uint32_t id, const std::string &data)
    {
        last[ObjectKey(kind, id)] = LastObject{{data, data}, _step};
    });
}

bool Recorder::WriteState(NC_STACK_ypaworld *world, const std::string &path, const std::string &startedFrom)
{
    Recorder recorder;

    if ( !recorder.Open(world, path, startedFrom, State::Capture(world, State::PART_VIEW),
                        State::Capture(world, State::PART_INTERNAL)) )
        return false;

    recorder._file->popChunk();
    recorder._file.reset();
    return true;
}

bool Recorder::SaveTo(NC_STACK_ypaworld *world, const std::string &path)
{
    if ( !_file )
        return false;

    // Everything up to now in the file, the last step one to go on from
    MakeResumable(world);
    FlushPending();

    if ( _blockSteps > 0 )
        WriteBlock();

    _fileHandle->flush();

    std::unique_ptr<FSMgr::FileHandle> in(OpenFile(_path, "rb"));
    if ( !in )
        return false;

    std::string data;
    char buf[65536];
    size_t got;
    while ( (got = in->read(buf, sizeof(buf))) > 0 )
        data.append(buf, got);

    if ( data.size() < 12 )
        return false;

    // A whole FORM, unlike the one still being recorded
    uint32_t size = data.size() - 8;
    uint8_t sizeBytes[4] = {(uint8_t)(size >> 24), (uint8_t)(size >> 16), (uint8_t)(size >> 8), (uint8_t)size};
    memcpy(&data[4], sizeBytes, 4);

    std::unique_ptr<FSMgr::FileHandle> out(OpenFile(path, "wb"));
    if ( !out || out->write(data.data(), data.size()) != data.size() )
    {
        ypa_log_out("Unable to write the saved game %s\n", path.c_str());
        return false;
    }

    return true;
}

bool Recorder::Start(NC_STACK_ypaworld *world, const std::string &path, const std::string &startedFrom)
{
    if ( !Open(world, path, startedFrom, State::Capture(world, State::PART_VIEW), State::Capture(world, State::PART_INTERNAL)) )
        return false;

    Remember(world, State::PART_VIEW, _lastView);
    Remember(world, State::PART_INTERNAL, _lastInternal);
    return true;
}

bool Recorder::StartFrom(NC_STACK_ypaworld *world, const std::string &path, const std::string &startedFrom,
                         const RecordedPoint &start, const RecordedPoint &point)
{
    if ( !Open(world, path, startedFrom, start.View, start.Internal) )
        return false;

    for ( const std::string &step : point.Steps )
        AddStep(step);

    // Where the recorded steps led, as the recording has it: the changes to
    // come are written from there
    _step = point.Step;
    _lastView.clear();
    _lastInternal.clear();

    for ( const auto &it : point.ViewVersions )
        _lastView[it.first] = LastObject{it.second, _step};

    for ( const auto &it : point.InternalVersions )
        _lastInternal[it.first] = LastObject{it.second, _step};

    return true;
}

// Where each kind's fields start, when all of them have a fixed size, as
// in the view: worked out once
Recorder::Layouts Recorder::MakeLayouts(const State::Schema &schema)
{
    Layouts layouts;

    for ( const auto &it : schema )
    {
        Layout &layout = layouts[it.first];
        layout.Fields = &it.second;
        layout.Fixed = true;
        layout.Offsets.push_back(0);

        for ( const State::FieldInfo &f : it.second )
        {
            if ( f.List )
            {
                layout.Fixed = false;
                layout.Offsets.clear();
                break;
            }

            layout.Offsets.push_back(layout.Offsets.back() + State::ElemSize(f.ElemType) * f.Count);
        }
    }

    return layouts;
}

std::string Recorder::Changes(NC_STACK_ypaworld *world, State::Part part, const State::Schema &schema,
                              const Layouts &layouts, LastObjects &last)
{
    // The count of changes first, filled in at the end
    std::string changes(4, '\0');
    uint32_t changeCount = 0;
    size_t seen = 0;
    std::vector<size_t> oldOffsets, newOffsets, beforeOffsets;

    State::ForEachObject(world, part, [&](uint8_t kind, uint32_t id, const std::string &data)
    {
        uint64_t key = ObjectKey(kind, id);
        auto it = last.find(key);

        if ( it == last.end() )
        {
            Put8(changes, kind);
            Put32(changes, id);
            Put8(changes, 1);
            Put32(changes, data.size());
            changes += data;
            changeCount++;
            seen++;

            last[key] = LastObject{{data, data}, _step};
            return;
        }

        LastObject &obj = it->second;
        obj.SeenStep = _step;
        seen++;

        if ( obj.Versions.Data == data )
            return;

        Put8(changes, kind);
        Put32(changes, id);

        auto lit = layouts.find(kind);
        bool fits = false;
        const std::vector<size_t> *before = NULL;

        if ( lit != layouts.end() )
        {
            const Layout &layout = lit->second;

            if ( layout.Fixed )
            {
                size_t size = layout.Offsets.back();
                fits = obj.Versions.Data.size() == size && data.size() == size;

                if ( fits )
                {
                    if ( obj.Versions.Before.size() == size )
                        before = &layout.Offsets;

                    Put8(changes, 2);
                    PutChangedFields(changes, *layout.Fields, obj.Versions, layout.Offsets, data, layout.Offsets, before);
                }
            }
            else if ( State::FieldOffsets(*layout.Fields, obj.Versions.Data, oldOffsets) &&
                      State::FieldOffsets(*layout.Fields, data, newOffsets) )
            {
                fits = true;

                if ( State::FieldOffsets(*layout.Fields, obj.Versions.Before, beforeOffsets) )
                    before = &beforeOffsets;

                Put8(changes, 2);
                PutChangedFields(changes, *layout.Fields, obj.Versions, oldOffsets, data, newOffsets, before);
            }
        }

        if ( !fits )
        {
            // Bytes that don't fit the fields, which shouldn't be: in full
            Put8(changes, 1);
            Put32(changes, data.size());
            changes += data;
        }

        changeCount++;
        obj.Versions.Before.swap(obj.Versions.Data);
        obj.Versions.Data.assign(data);
    }, &schema);

    // The objects gone, in order of kind and id
    if ( seen < last.size() )
    {
        std::vector<uint64_t> gone;
        for ( const auto &it : last )
        {
            if ( it.second.SeenStep != _step )
                gone.push_back(it.first);
        }

        std::sort(gone.begin(), gone.end());

        for ( uint64_t key : gone )
        {
            Put8(changes, key >> 32);
            Put32(changes, key & 0xFFFFFFFF);
            Put8(changes, 0);
            changeCount++;
            last.erase(key);
        }
    }

    memcpy(&changes[0], &changeCount, 4);
    return changes;
}

void Recorder::Step(NC_STACK_ypaworld *world)
{
    if ( !_file )
        return;

    FlushPending();

    _step++;

    _pendingView = Changes(world, State::PART_VIEW, _viewSchema, _viewLayouts, _lastView);

    _pendingMessages.clear();
    Put16(_pendingMessages, _messageCount);
    _pendingMessages += _messages;
    _messages.clear();
    _messageCount = 0;

    _pendingResumable = false;
    _pendingInternal.clear();
    _hasPending = true;

    if ( _full || _step % BlockSteps == 0 )
        MakeResumable(world);
}

void Recorder::MakeResumable(NC_STACK_ypaworld *world)
{
    if ( !_hasPending || _pendingResumable )
        return;

    _pendingInternal = Changes(world, State::PART_INTERNAL, _internalSchema, _internalLayouts, _lastInternal);
    _pendingResumable = true;
}

void Recorder::FlushPending()
{
    if ( !_hasPending )
        return;

    std::string step;
    Put8(step, _pendingResumable ? 1 : 0);
    step += _pendingView;
    step += _pendingMessages;
    step += _pendingInternal;

    _hasPending = false;
    AddStep(step);
}

// Packs what comes into the block's zlib stream, a step at a time, so that
// the packing doesn't stop the game for a whole block at its end
void Recorder::Deflate(const void *data, size_t size, int flush)
{
    if ( !_packer )
    {
        _packer.reset(new z_stream());
        deflateInit(_packer.get(), Z_DEFAULT_COMPRESSION);
        _packed.clear();
        _unpackedSize = 0;
    }

    z_stream &zs = *_packer;
    zs.next_in = (Bytef *)data;
    zs.avail_in = size;
    _unpackedSize += size;

    char out[16384];

    do
    {
        zs.next_out = (Bytef *)out;
        zs.avail_out = sizeof(out);

        if ( deflate(&zs, flush) == Z_STREAM_ERROR )
        {
            ypa_log_out("Recording: could not compress a block\n");
            break;
        }

        _packed.append(out, sizeof(out) - zs.avail_out);
    }
    while ( zs.avail_out == 0 || (flush == Z_FINISH && zs.avail_in > 0) );
}

void Recorder::AddStep(const std::string &step)
{
    uint32_t size = step.size();
    Deflate(&size, 4, Z_NO_FLUSH);
    Deflate(step.data(), step.size(), Z_NO_FLUSH);
    _blockSteps++;

    if ( _blockSteps >= BlockSteps )
        WriteBlock();
}

void Recorder::Message(const TInfoMessage &msg)
{
    if ( !_file )
        return;

    Put32(_messages, msg.MsgID);
    Put16(_messages, msg.Priority);
    Put32(_messages, msg.unit ? msg.unit->_gid : 0);
    PutText(_messages, msg.txt);
    _messageCount++;
}

void Recorder::Stop(NC_STACK_ypaworld *world)
{
    if ( !_file )
        return;

    MakeResumable(world);
    FlushPending();

    if ( _blockSteps > 0 )
        WriteBlock();

    _file->popChunk();
    _file.reset();
    _fileHandle = NULL;
    _lastView.clear();
    _lastInternal.clear();
}

void Recorder::WriteBlock()
{
    Deflate(NULL, 0, Z_FINISH);
    deflateEnd(_packer.get());
    _packer.reset();

    _file->pushChunk(0, TAG_STEP, -1);
    _file->writeU32L(_unpackedSize);
    _file->write(_packed.data(), _packed.size());
    _file->popChunk();

    _packed.clear();
    _blockSteps = 0;
}


// Recording

static uint32_t BigEndian32(const std::string &data, size_t pos)
{
    const uint8_t *p = (const uint8_t *)data.data() + pos;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

bool Recording::Open(const std::string &path)
{
    std::unique_ptr<FSMgr::FileHandle> file(OpenFile(path, "rb"));

    if ( !file )
    {
        ypa_log_out("Can't read the recording %s\n", path.c_str());
        return false;
    }

    std::string data;
    char buf[65536];
    size_t got;
    while ( (got = file->read(buf, sizeof(buf))) > 0 )
        data.append(buf, got);

    if ( data.size() < 12 || data.compare(0, 4, "FORM") || data.compare(8, 4, "RPLY") )
    {
        ypa_log_out("%s is not a recording\n", path.c_str());
        return false;
    }

    bool haveInfo = false, haveSchemas = true;
    size_t pos = 12;

    while ( pos + 8 <= data.size() )
    {
        std::string tag = data.substr(pos, 4);
        uint32_t size = BigEndian32(data, pos + 4);

        // Cut short: the rest is the chunk being written, if anything
        if ( size == 0xFFFFFFFF || pos + 8 + size > data.size() )
            break;

        std::string body = data.substr(pos + 8, size);
        pos += 8 + size + (size & 1);

        if ( tag == "INFO" )
        {
            if ( body.size() < 4 )
                break;

            uint16_t version, level;
            memcpy(&version, body.data(), 2);
            memcpy(&level, body.data() + 2, 2);

            if ( version != 2 )
            {
                ypa_log_out("%s is a recording of format %d, not 2\n", path.c_str(), version);
                return false;
            }

            _level = level;
            _startedFrom = std::string(body.c_str() + 4);
            haveInfo = true;
        }
        else if ( tag == "SCHV" )
        {
            haveSchemas &= State::ReadSchema(body, _viewSchema);
        }
        else if ( tag == "SCHI" )
        {
            haveSchemas &= State::ReadSchema(body, _internalSchema);
        }
        else if ( tag == "STRV" )
        {
            _startView = std::move(body);
        }
        else if ( tag == "STRI" )
        {
            _startInternal = std::move(body);
        }
        else if ( tag == "STEP" )
        {
            std::string steps;
            if ( !Unpacked(body, steps) )
                break;

            for (size_t at = 0; at + 5 <= steps.size(); )
            {
                uint32_t stepSize;
                memcpy(&stepSize, steps.data() + at, 4);
                _resumable.push_back(steps[at + 4] != 0);
                at += 4 + stepSize;
            }

            _blocks.push_back(std::move(body));
        }
    }

    if ( !haveInfo || !haveSchemas || _viewSchema.empty() || _startView.empty() || _startInternal.empty() )
    {
        ypa_log_out("%s is not a whole recording\n", path.c_str());
        return false;
    }

    return true;
}

uint32_t Recording::LastResumable(uint32_t step) const
{
    step = std::min<uint32_t>(step, _resumable.size());

    while ( step > 0 && !_resumable[step - 1] )
        step--;

    return step;
}

static void ToImage(int level, const ObjectsVersions &objects, State::Image &image)
{
    image = State::Image();
    image.Level = level;

    for ( auto &it : objects )
    {
        State::Object obj;
        obj.Kind = it.first >> 32;
        obj.Id = it.first & 0xFFFFFFFF;
        obj.Data = it.second.Data;
        image.Objects.push_back(std::move(obj));
    }
}

bool Recording::Read(uint32_t step, RecordedPoint &point, bool resume) const
{
    if ( step > Steps() || (resume && !IsResumable(step)) )
        return false;

    point = RecordedPoint();
    point.Step = step;

    ObjectsVersions &viewObjects = point.ViewVersions;
    ObjectsVersions &internalObjects = point.InternalVersions;

    for (auto part : {std::make_pair(&_startView, &viewObjects), std::make_pair(&_startInternal, &internalObjects)})
    {
        State::Image start;
        if ( !State::Unpack(*part.first, start) )
            return false;

        for ( State::Object &obj : start.Objects )
            (*part.second)[ObjectKey(obj.Kind, obj.Id)] = ObjectVersions{obj.Data, obj.Data};
    }

    uint32_t done = 0;

    for (size_t b = 0; b < _blocks.size() && done < step; b++)
    {
        std::string block;
        if ( !Unpacked(_blocks[b], block) )
            return false;

        size_t at = 0;

        while ( done < step && at + 4 <= block.size() )
        {
            uint32_t stepSize;
            memcpy(&stepSize, block.data() + at, 4);

            if ( stepSize < 1 || at + 4 + stepSize > block.size() )
                return false;

            const char *p = block.data() + at + 4;
            const char *end = p + stepSize;

            point.Steps.emplace_back(p, stepSize);

            bool resumable = *p++ != 0;

            p = ApplyChanges(viewObjects, _viewSchema, p, end);
            if ( !p || p + 2 > end )
                return false;

            // The messages
            uint16_t messages;
            memcpy(&messages, p, 2);
            p += 2;

            for (uint16_t m = 0; m < messages; m++)
            {
                if ( p + 12 > end )
                    return false;

                uint16_t length;
                memcpy(&length, p + 10, 2);
                p += 12 + length;
            }

            if ( resumable && resume )
            {
                p = ApplyChanges(internalObjects, _internalSchema, p, end);
                if ( !p )
                    return false;
            }

            at += 4 + stepSize;
            done++;
        }
    }

    if ( done < step )
        return false;

    ToImage(_level, viewObjects, point.View);

    if ( resume )
        ToImage(_level, internalObjects, point.Internal);
    else
        internalObjects.clear();

    return true;
}

}
