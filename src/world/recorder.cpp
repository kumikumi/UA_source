#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <zlib.h>

#include "recorder.h"
#include "../yw.h"
#include "../utils.h"
#include "../env.h"
#include "../log.h"
#include "../fmtlib/printf.h"

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

static void Put64(std::string &out, uint64_t v)
{
    out.append((const char *)&v, 8);
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
// Whether a float or double is finite, by its exponent bits: no function
// call per element in a build without optimization
static inline bool Finite(float v)
{
    uint32_t bits;
    memcpy(&bits, &v, 4);
    return (bits & 0x7F800000) != 0x7F800000;
}

static inline bool Finite(double v)
{
    uint64_t bits;
    memcpy(&bits, &v, 8);
    return (bits & 0x7FF0000000000000ULL) != 0x7FF0000000000000ULL;
}

// The most bytes a field can have to be XORed with a guess; bigger ones are
// written as they are
static const size_t MaxGuessed = 256;

template <class T>
static void PredictElements(const char *last, const char *before, char *guess, size_t count)
{
    // Copied whole, as fields needn't be aligned: a copy a field, not one an
    // element. Fields are at most MaxGuessed bytes.
    T v1s[MaxGuessed / sizeof(T)], v0s[MaxGuessed / sizeof(T)];
    memcpy(v1s, last, count * sizeof(T));
    memcpy(v0s, before, count * sizeof(T));

    for (size_t i = 0; i < count; i++)
    {
        T v1 = v1s[i], v0 = v0s[i];

        if ( Finite(v1) && Finite(v0) )
        {
            T moved = v1 - v0;
            T next = v1 + moved;

            if ( Finite(moved) && Finite(next) )
                v1s[i] = next;
        }
    }

    memcpy(guess, v1s, count * sizeof(T));
}

// The same for fixed-point positions and quaternions, whose arithmetic
// wraps around: T is unsigned, of the elements' size
template <class T>
static void PredictIntegers(const char *last, const char *before, char *guess, size_t count)
{
    T v1s[MaxGuessed / sizeof(T)], v0s[MaxGuessed / sizeof(T)];
    memcpy(v1s, last, count * sizeof(T));
    memcpy(v0s, before, count * sizeof(T));

    for (size_t i = 0; i < count; i++)
        v1s[i] = v1s[i] + (T)(v1s[i] - v0s[i]);

    memcpy(guess, v1s, count * sizeof(T));
}

// What a field's new bytes are written against: the guess for floats,
// positions and quaternions, else the last bytes
static void Guess(const State::FieldInfo &field, const char *last, const char *before, size_t size, char *guess)
{
    if ( before && field.ElemType == State::T_F32 )
        PredictElements<float>(last, before, guess, size / 4);
    else if ( before && field.ElemType == State::T_F64 )
        PredictElements<double>(last, before, guess, size / 8);
    else if ( before && field.ElemType == State::T_FIXED )
        PredictIntegers<uint32_t>(last, before, guess, size / 4);
    else if ( before && field.ElemType == State::T_QUAT )
        PredictIntegers<uint16_t>(last, before, guess, size / 2);
    else
        memcpy(guess, last, size);
}

// How a field's new bytes are written against their guess, and read back:
// positions and quaternions as their difference from it, which is small
// when the guess is close; anything else XORed with it, which is mostly
// zeros then
template <class T>
static void Subtract(const char *a, const char *b, char *out, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        T x, y;
        memcpy(&x, a + i * sizeof(T), sizeof(T));
        memcpy(&y, b + i * sizeof(T), sizeof(T));
        x -= y;
        memcpy(out + i * sizeof(T), &x, sizeof(T));
    }
}

template <class T>
static void Add(const char *a, const char *b, char *out, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        T x, y;
        memcpy(&x, a + i * sizeof(T), sizeof(T));
        memcpy(&y, b + i * sizeof(T), sizeof(T));
        x += y;
        memcpy(out + i * sizeof(T), &x, sizeof(T));
    }
}

static void Encode(const State::FieldInfo &field, const char *guess, const char *bytes, size_t size, char *out)
{
    if ( field.ElemType == State::T_FIXED )
        Subtract<uint32_t>(bytes, guess, out, size / 4);
    else if ( field.ElemType == State::T_QUAT )
        Subtract<uint16_t>(bytes, guess, out, size / 2);
    else
    {
        for (size_t b = 0; b < size; b++)
            out[b] = guess[b] ^ bytes[b];
    }
}

static void Decode(const State::FieldInfo &field, const char *guess, const char *stored, size_t size, char *out)
{
    if ( field.ElemType == State::T_FIXED )
        Add<uint32_t>(guess, stored, out, size / 4);
    else if ( field.ElemType == State::T_QUAT )
        Add<uint16_t>(guess, stored, out, size / 2);
    else
    {
        for (size_t b = 0; b < size; b++)
            out[b] = guess[b] ^ stored[b];
    }
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

    char guess[MaxGuessed];

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
            Encode(fields[i], guess, n, newSize, &out[at]);
        }

        count++;
    }

    memcpy(&out[countAt], &count, 2);
}

// Applies what changed, as the format says, to objects by key. Returns where
// it ended, or NULL when it doesn't read.
static const char *ApplyChanges(std::map<uint64_t, ObjectVersions> &objects, const KindLayouts &layouts,
                                const char *p, const char *end, ChangedObjects *changed = NULL)
{
    std::vector<size_t> offsets, beforeOffsets;
    std::string scratch;
    char guess[MaxGuessed];

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

            if ( changed )
                changed->emplace_back(key, nullptr);
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

            if ( changed )
                changed->emplace_back(key, &obj);
        }
        else
        {
            auto lit = layouts.find(kind);
            auto oit = objects.find(key);

            if ( lit == layouts.end() || oit == objects.end() || p + 2 > end )
                return NULL;

            const KindLayout &layout = lit->second;
            const std::vector<State::FieldInfo> &fields = *layout.Fields;
            ObjectVersions &obj = oit->second;

            if ( changed )
                changed->emplace_back(key, &obj);

            uint16_t n;
            memcpy(&n, p, 2);
            p += 2;

            if ( layout.Fixed )
            {
                // In place: the last bytes become the ones before, and the
                // changed fields are written over them; the guesses need the
                // bytes before the last too, which go to the scratch buffer
                size_t size = layout.Offsets.back();
                if ( obj.Data.size() != size )
                    return NULL;

                scratch.swap(obj.Before);
                obj.Before.assign(obj.Data);
                bool beforeFits = scratch.size() == size;

                for (uint16_t i = 0; i < n; i++)
                {
                    uint16_t index;
                    if ( p + 2 > end )
                        return NULL;

                    memcpy(&index, p, 2);
                    p += 2;

                    if ( index >= fields.size() )
                        return NULL;

                    size_t at = layout.Offsets[index];
                    size_t fieldSize = layout.Offsets[index + 1] - at;

                    if ( p + fieldSize > end )
                        return NULL;

                    char *dst = &obj.Data[at];

                    if ( fieldSize > sizeof(guess) )
                    {
                        memcpy(dst, p, fieldSize);
                    }
                    else
                    {
                        Guess(fields[index], obj.Before.data() + at, beforeFits ? scratch.data() + at : NULL, fieldSize, guess);
                        Decode(fields[index], guess, p, fieldSize, dst);
                    }

                    p += fieldSize;
                }

                continue;
            }

            if ( !State::FieldOffsets(fields, obj.Data, offsets) )
                return NULL;

            const std::string &old = obj.Data;
            bool beforeFits = State::FieldOffsets(fields, obj.Before, beforeOffsets);

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
                          BeforeBytes(fields[index], obj.Before, beforeOffsets, beforeFits, index, size), size, guess);

                    char value[MaxGuessed];
                    Decode(fields[index], guess, p, size, value);
                    updated.append(value, size);
                }

                p += size;
                field = index + 1;
            }

            updated.append(old, offsets[field], old.size() - offsets[field]);
            obj.Before = std::move(obj.Data);
            obj.Data = std::move(updated);
        }
    }

    return p;
}



// Recordings' files

static const char Magic[6] = {'U', 'A', 'R', 'E', 'C', '\0'};
static const uint16_t FormatVersion = 3;

// How many bytes of a keyframe are packed in a step, after its first: a
// keyframe takes some tens of milliseconds to pack whole
static const size_t KeyframeSlice = 32768;

// Two paths to the same file, as the game's folder has them
static bool SameFile(const std::string &a, const std::string &b)
{
    return Common::Env.ApplyPrefix(a) == Common::Env.ApplyPrefix(b);
}

// Copies the first size bytes of one file into another, which is made anew
static bool CopyFile(const std::string &from, const std::string &to, uint64_t size)
{
    std::unique_ptr<FSMgr::FileHandle> in(OpenFile(from, "rb"));
    std::unique_ptr<FSMgr::FileHandle> out(OpenFile(to, "wb"));

    if ( !in || !out )
        return false;

    std::vector<char> buf(1 << 20);

    while ( size > 0 )
    {
        size_t n = std::min<uint64_t>(size, buf.size());

        if ( in->read(buf.data(), n) != n || out->write(buf.data(), n) != n )
            return false;

        size -= n;
    }

    return true;
}

// Reads a chunk's bytes, after its tag and size, from where it starts
static bool ReadChunk(FSMgr::FileHandle &file, uint64_t start, uint64_t end, std::string &data)
{
    if ( end < start + 12 || file.seek(start + 12, SEEK_SET) != 0 )
        return false;

    data.resize(end - start - 12);
    return data.empty() || file.read(&data[0], data.size()) == data.size();
}

// A keyframe's chunk: its view, and its internal part unless that's NULL
static bool ReadKeyframeChunk(const std::string &path, const KeyframeAt &at, State::Image *view, State::Image *internal)
{
    std::unique_ptr<FSMgr::FileHandle> file(OpenFile(path, "rb"));
    std::string data, raw;

    if ( !file || !ReadChunk(*file, at.Start, at.End, data) || data.size() < 8 || !Unpacked(data.substr(4), raw) )
        return false;

    size_t pos = 0;

    if ( !State::Deserialize(raw, pos, *view) )
        return false;

    return !internal || (State::Deserialize(raw, pos, *internal) && pos == raw.size());
}

// The objects in an image's bytes, in the order of their keys, as State
// Serialize writes them
static void SerializeObjects(int level, const std::vector<std::pair<uint64_t, const std::string *>> &objects,
                             std::string &out)
{
    Put32(out, level);
    Put32(out, objects.size());

    for ( const auto &obj : objects )
    {
        Put8(out, obj.first >> 32);
        Put32(out, obj.first & 0xFFFFFFFF);
        Put32(out, obj.second->size());
        out += *obj.second;
    }
}


// Recorder::Packer

Recorder::Packer::Packer() {}

Recorder::Packer::~Packer()
{
    Drop();
}

void Recorder::Packer::Deflate(const void *data, size_t size, int flush)
{
    if ( !_stream )
    {
        _stream.reset(new z_stream());
        deflateInit(_stream.get(), Z_DEFAULT_COMPRESSION);
        _packed.clear();
        _unpacked = 0;
    }

    z_stream &zs = *_stream;
    zs.next_in = (Bytef *)data;
    zs.avail_in = size;
    _unpacked += size;

    char out[16384];

    do
    {
        zs.next_out = (Bytef *)out;
        zs.avail_out = sizeof(out);

        if ( deflate(&zs, flush) == Z_STREAM_ERROR )
        {
            ypa_log_out("Recording: could not compress\n");
            break;
        }

        _packed.append(out, sizeof(out) - zs.avail_out);
    }
    while ( zs.avail_out == 0 || (flush == Z_FINISH && zs.avail_in > 0) );
}

void Recorder::Packer::Add(const void *data, size_t size)
{
    Deflate(data, size, Z_NO_FLUSH);
}

std::string Recorder::Packer::Finish()
{
    Deflate(NULL, 0, Z_FINISH);
    deflateEnd(_stream.get());
    _stream.reset();

    std::string packed;
    packed.swap(_packed);
    return packed;
}

void Recorder::Packer::Drop()
{
    if ( _stream )
    {
        deflateEnd(_stream.get());
        _stream.reset();
    }

    _packed.clear();
    _unpacked = 0;
}


// Recorder

Recorder::Recorder() {}
Recorder::~Recorder() {}

void Recorder::Reset()
{
    _cutPending = false;
    _steps.Drop();
    _blockSteps = 0;
    _keyPacker.Drop();
    _keyRaw.clear();
    _keyAt = 0;
    _keyPending = false;
    _messages.clear();
    _messageCount = 0;
    _lastView.clear();
    _keyframes.clear();
}

bool Recorder::WriteChunk(const char *tag, const std::string &head, const std::string &data)
{
    std::string header(tag, 4);
    Put64(header, head.size() + data.size());

    return _file->write(header.data(), header.size()) == header.size() &&
           _file->write(head.data(), head.size()) == head.size() &&
           _file->write(data.data(), data.size()) == data.size();
}

bool Recorder::Open(NC_STACK_ypaworld *world, const std::string &path, const std::string &startedFrom)
{
    FSMgr::FileHandle *file = OpenFile(path, "wb");

    if ( !file )
    {
        ypa_log_out("Unable to write the recording %s\n", path.c_str());
        return false;
    }

    Reset();
    _path = path;
    _file.reset(file);
    _level = world->_levelInfo.LevelID;
    _step = world->_framesElapsed;

    std::string header(Magic, sizeof(Magic));
    Put16(header, FormatVersion);
    _file->write(header.data(), header.size());

    std::string info;
    Put16(info, _level);

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

    WriteChunk("INFO", std::string(), info);
    WriteChunk("SCHV", std::string(), State::WriteSchema(_viewSchema));
    WriteChunk("SCHI", std::string(), State::WriteSchema(_internalSchema));
    return true;
}

void Recorder::Remember(NC_STACK_ypaworld *world)
{
    _lastView.clear();

    State::ForEachObject(world, State::PART_VIEW, [this](uint8_t kind, uint32_t id, const std::string &data)
    {
        _lastView[ObjectKey(kind, id)] = LastObject{{data, data}, _step};
    }, &_viewSchema);
}

void Recorder::RememberImage(const State::Image &view)
{
    _lastView.clear();

    for ( const State::Object &obj : view.Objects )
        _lastView[ObjectKey(obj.Kind, obj.Id)] = LastObject{{obj.Data, obj.Data}, _step};
}

bool Recorder::Start(NC_STACK_ypaworld *world, const std::string &path, const std::string &startedFrom)
{
    if ( !Open(world, path, startedFrom) )
        return false;

    Remember(world);

    // The level is still loading: the first keyframe is written at once
    MakeKeyframe(world);
    PackKeyframe(SIZE_MAX);
    return true;
}

bool Recorder::ContinueFrom(NC_STACK_ypaworld *world, const std::string &path, const Recording &source, uint32_t step)
{
    const KeyframeAt *at = source.KeyframeAtOrBefore(step);
    State::Image view, internal;

    if ( !at || at->Step != step || !source.ReadKeyframe(step, view, internal) )
    {
        ypa_log_out("Recording: %s has no keyframe at step %d to go on from\n", source.Path().c_str(), step);
        return false;
    }

    State::Schema viewSchema = State::MakeSchema(world, State::PART_VIEW);
    State::Schema internalSchema = State::MakeSchema(world, State::PART_INTERNAL);

    if ( !source.SameSchemas(viewSchema, internalSchema) )
    {
        ypa_log_out("Recording: %s was recorded by a build whose state differs, so a new recording starts at step %d\n",
                    source.Path().c_str(), step);
        return Start(world, path, fmt::sprintf("%s at step %d", source.Path(), step));
    }

    // The source up to the keyframe, then what comes
    FSMgr::FileHandle *file = NULL;

    if ( SameFile(path, source.Path()) )
    {
        file = OpenFile(path, "r+b");

        if ( file && !file->truncate(at->End) )
        {
            delete file;
            file = NULL;
        }
    }
    else if ( CopyFile(source.Path(), path, at->End) )
    {
        file = OpenFile(path, "r+b");
    }

    if ( !file )
    {
        ypa_log_out("Unable to go on recording %s from %s\n", path.c_str(), source.Path().c_str());
        return false;
    }

    Reset();
    _path = path;
    _file.reset(file);
    _file->seek(0, SEEK_END);
    _level = source.Level();
    _viewSchema = viewSchema;
    _internalSchema = internalSchema;
    _viewLayouts = MakeLayouts(_viewSchema);
    _step = step;
    _blockStart = step + 1;

    for ( const KeyframeAt &k : source.Keyframes() )
    {
        if ( k.Step <= step )
            _keyframes.push_back(k);
    }

    RememberImage(view);
    return true;
}

bool Recorder::WriteState(NC_STACK_ypaworld *world, const std::string &path, const std::string &startedFrom)
{
    Recorder recorder;

    if ( !recorder.Start(world, path, startedFrom) )
        return false;

    recorder._file.reset();
    return true;
}

// Where each kind's fields start, when all of them have a fixed size, as
// in the view: worked out once
KindLayouts MakeLayouts(const State::Schema &schema)
{
    KindLayouts layouts;

    for ( const auto &it : schema )
    {
        KindLayout &layout = layouts[it.first];
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
                              const KindLayouts &layouts, LastObjects &last)
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
            const KindLayout &layout = lit->second;

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

    Cut();
    _step++;

    std::string step = Changes(world, State::PART_VIEW, _viewSchema, _viewLayouts, _lastView);
    Put16(step, _messageCount);
    step += _messages;
    _messages.clear();
    _messageCount = 0;

    uint32_t size = step.size();
    _steps.Add(&size, 4);
    _steps.Add(step.data(), step.size());
    _blockSteps++;

    if ( _full || _step % KeyframeSteps == 0 )
    {
        EndBlock();
        MakeKeyframe(world);

        if ( _full )
            PackKeyframe(SIZE_MAX);
    }
    else
    {
        PackKeyframe(KeyframeSlice);
    }
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

// Writes the steps since the last keyframe, after it
void Recorder::EndBlock()
{
    PackKeyframe(SIZE_MAX);

    if ( !_blockSteps )
        return;

    uint64_t unpacked = _steps.Unpacked();
    std::string packed = _steps.Finish();

    std::string head;
    Put32(head, _blockStart);
    Put32(head, _blockSteps);
    Put32(head, unpacked);
    WriteChunk("STEP", head, packed);

    _blockSteps = 0;
}

// A keyframe of the level as it is after the last step: its bytes now, packed
// a piece every step until it's written (PackKeyframe). The steps after it
// start a block.
void Recorder::MakeKeyframe(NC_STACK_ypaworld *world)
{
    PackKeyframe(SIZE_MAX);

    // The view as the steps wrote it, in the order of the objects' keys, as
    // the internal part's objects are; the guesses of its changes start over
    std::vector<std::pair<uint64_t, const std::string *>> view;
    view.reserve(_lastView.size());

    for ( auto &it : _lastView )
    {
        it.second.Versions.Before = it.second.Versions.Data;
        view.emplace_back(it.first, &it.second.Versions.Data);
    }

    std::sort(view.begin(), view.end());

    _keyRaw.clear();
    SerializeObjects(_level, view, _keyRaw);

    // The internal part, from the level
    Put32(_keyRaw, _level);
    size_t countAt = _keyRaw.size();
    Put32(_keyRaw, 0);
    uint32_t count = 0;

    State::ForEachObject(world, State::PART_INTERNAL, [this, &count](uint8_t kind, uint32_t id, const std::string &data)
    {
        Put8(_keyRaw, kind);
        Put32(_keyRaw, id);
        Put32(_keyRaw, data.size());
        _keyRaw += data;
        count++;
    }, &_internalSchema);

    memcpy(&_keyRaw[countAt], &count, 4);

    _keyAt = 0;
    _keyStep = _step;
    _keyPending = true;
    _blockStart = _step + 1;
}

// Packs on up to that many bytes of the keyframe made last; once it's
// packed, it's written
void Recorder::PackKeyframe(size_t bytes)
{
    if ( !_keyPending )
        return;

    size_t n = std::min(bytes, _keyRaw.size() - _keyAt);
    _keyPacker.Add(_keyRaw.data() + _keyAt, n);
    _keyAt += n;

    if ( _keyAt < _keyRaw.size() )
        return;

    uint64_t unpacked = _keyPacker.Unpacked();
    std::string packed = _keyPacker.Finish();

    std::string head;
    Put32(head, _keyStep);
    Put32(head, unpacked);

    KeyframeAt at;
    at.Step = _keyStep;
    at.Start = _file->tell();
    WriteChunk("KEYF", head, packed);
    at.End = _file->tell();
    _keyframes.push_back(at);

    _keyPending = false;
    _keyRaw.clear();
}

std::string Recorder::Stop(NC_STACK_ypaworld *world, RecordingEnd end)
{
    if ( !_file )
        return std::string();

    Cut();
    EndBlock();

    if ( _keyframes.empty() || _keyframes.back().Step != _step )
    {
        MakeKeyframe(world);
        PackKeyframe(SIZE_MAX);
    }

    std::string how;
    Put8(how, end);
    WriteChunk("END ", std::string(), how);

    _file.reset();
    Reset();
    return _path;
}

bool Recorder::SaveTo(NC_STACK_ypaworld *world, const std::string &path)
{
    if ( !_file )
        return false;

    // Everything up to now in the file, ending with a keyframe of now
    Cut();
    EndBlock();

    if ( _keyframes.empty() || _keyframes.back().Step != _step )
    {
        MakeKeyframe(world);
        PackKeyframe(SIZE_MAX);
    }

    _file->flush();

    if ( !CopyFile(_path, path, _file->tell()) )
    {
        ypa_log_out("Unable to write the saved game %s\n", path.c_str());
        return false;
    }

    return true;
}

bool Recorder::RewindTarget(uint32_t minSteps, uint32_t &step) const
{
    // The keyframe being packed is one too
    if ( _keyPending && _keyStep + minSteps <= _step )
    {
        step = _keyStep;
        return true;
    }

    for (size_t n = _keyframes.size(); n > 0; n--)
    {
        if ( _keyframes[n - 1].Step + minSteps <= _step )
        {
            step = _keyframes[n - 1].Step;
            return true;
        }
    }

    return false;
}

bool Recorder::KeyframeBefore(uint32_t step, uint32_t &keyframe) const
{
    for (size_t n = _keyframes.size(); n > 0; n--)
    {
        if ( _keyframes[n - 1].Step < step )
        {
            keyframe = _keyframes[n - 1].Step;
            return true;
        }
    }

    return false;
}

void Recorder::Flush()
{
    if ( _file )
        _file->flush();
}

// Where Rewind went back to, the file ends
void Recorder::Cut()
{
    if ( !_cutPending )
        return;

    _cutPending = false;

    if ( !_file->truncate(_cutAt) )
        ypa_log_out("Recording: could not cut %s back\n", _path.c_str());
}

bool Recorder::Rewind(uint32_t minSteps, State::Image &view, State::Image &internal)
{
    if ( !_file )
        return false;

    // The latest keyframe, should it be the one, written whole
    Cut();
    PackKeyframe(SIZE_MAX);

    size_t n = _keyframes.size();
    while ( n > 0 && _keyframes[n - 1].Step + minSteps > _step )
        n--;

    if ( n == 0 )
        return false;

    const KeyframeAt at = _keyframes[n - 1];
    _file->flush();

    if ( !ReadKeyframeChunk(_path, at, &view, &internal) )
    {
        ypa_log_out("Recording: could not read %s back at step %d\n", _path.c_str(), at.Step);
        return false;
    }

    // The file is cut after it before it's written again: until then, what
    // came after it can still be read
    _cutPending = true;
    _cutAt = at.End;

    // What came after it goes: the steps since the last keyframe and their
    // messages
    _steps.Drop();
    _blockSteps = 0;
    _messages.clear();
    _messageCount = 0;
    _keyframes.resize(n);

    _step = at.Step;
    _blockStart = _step + 1;
    RememberImage(view);
    return true;
}


// Recording

bool Recording::Open(const std::string &path)
{
    *this = Recording();
    _path = path;

    std::unique_ptr<FSMgr::FileHandle> file(OpenFile(path, "rb"));

    if ( !file )
    {
        ypa_log_out("Can't read the recording %s\n", path.c_str());
        return false;
    }

    file->seek(0, SEEK_END);
    uint64_t fileSize = file->tell();
    file->seek(0, SEEK_SET);

    char header[8];
    uint16_t version;

    if ( file->read(header, 8) != 8 || memcmp(header, Magic, sizeof(Magic)) )
    {
        ypa_log_out("%s is not a recording\n", path.c_str());
        return false;
    }

    memcpy(&version, header + 6, 2);

    if ( version != FormatVersion )
    {
        ypa_log_out("%s is a recording of format %d, not %d\n", path.c_str(), version, FormatVersion);
        return false;
    }

    bool haveInfo = false;
    uint64_t pos = 8;

    // Where the chunks are, and the steps of keyframes and blocks: a
    // recording cut short ends with its last whole chunk, or where the
    // chunks don't follow each other
    while ( pos + 12 <= fileSize )
    {
        char chunkHead[12];
        uint64_t size;

        if ( file->seek(pos, SEEK_SET) != 0 || file->read(chunkHead, 12) != 12 )
            break;

        memcpy(&size, chunkHead + 4, 8);

        if ( size > fileSize - pos - 12 )
            break;

        std::string tag(chunkHead, 4);
        uint64_t start = pos;
        pos += 12 + size;

        if ( tag == "INFO" || tag == "SCHV" || tag == "SCHI" )
        {
            std::string body(size, '\0');
            if ( size && file->read(&body[0], size) != size )
                break;

            if ( tag == "INFO" )
            {
                if ( body.size() < 2 )
                    break;

                uint16_t level;
                memcpy(&level, body.data(), 2);
                _level = level;
                _startedFrom = std::string(body.c_str() + 2);
                haveInfo = true;
            }
            else if ( tag == "SCHV" )
            {
                _viewSchemaBytes = body;
            }
            else
            {
                _internalSchemaBytes = body;
            }
        }
        else if ( tag == "KEYF" )
        {
            uint32_t step;
            if ( size < 8 || file->read(&step, 4) != 4 )
                break;

            // After the steps before it, or first
            if ( !_keyframes.empty() && (_blocks.empty() || _blocks.back().FirstStep + _blocks.back().Steps - 1 != step ||
                                         _blocks.back().FirstStep != _keyframes.back().Step + 1) )
                break;

            KeyframeAt at;
            at.Step = step;
            at.Start = start;
            at.End = pos;
            _keyframes.push_back(at);
            _lastStep = step;
        }
        else if ( tag == "END " )
        {
            uint8_t end;
            if ( size < 1 || file->read(&end, 1) != 1 )
                break;

            _end = (RecordingEnd)end;
            break;
        }
        else if ( tag == "STEP" )
        {
            uint32_t head[2];
            if ( size < 12 || file->read(head, 8) != 8 )
                break;

            // The steps after the last keyframe, once
            if ( _keyframes.empty() || head[0] != _keyframes.back().Step + 1 || head[1] == 0 ||
                 (!_blocks.empty() && _blocks.back().FirstStep == head[0]) )
                break;

            Block block;
            block.FirstStep = head[0];
            block.Steps = head[1];
            block.Start = start;
            block.End = pos;
            _blocks.push_back(block);
            _lastStep = block.FirstStep + block.Steps - 1;
        }
    }

    if ( !haveInfo || !State::ReadSchema(_viewSchemaBytes, _viewSchema) || _viewSchema.empty() ||
         !State::ReadSchema(_internalSchemaBytes, _internalSchema) || _keyframes.empty() )
    {
        ypa_log_out("%s is not a whole recording\n", path.c_str());
        return false;
    }

    _viewLayouts = MakeLayouts(_viewSchema);
    return true;
}

const KeyframeAt *Recording::KeyframeAtOrBefore(uint32_t step) const
{
    const KeyframeAt *found = NULL;

    for ( const KeyframeAt &at : _keyframes )
    {
        if ( at.Step > step )
            break;

        found = &at;
    }

    return found;
}

bool Recording::ReadKeyframe(uint32_t step, State::Image &view, State::Image &internal) const
{
    const KeyframeAt *at = KeyframeAtOrBefore(step);

    if ( !at || at->Step != step )
    {
        ypa_log_out("%s has no keyframe at step %d\n", _path.c_str(), step);
        return false;
    }

    if ( !ReadKeyframeChunk(_path, *at, &view, &internal) )
    {
        ypa_log_out("Can't read %s at step %d\n", _path.c_str(), step);
        return false;
    }

    return true;
}

bool Recording::SameSchemas(const State::Schema &view, const State::Schema &internal) const
{
    return State::WriteSchema(view) == _viewSchemaBytes && State::WriteSchema(internal) == _internalSchemaBytes;
}

bool Recording::ReadBlock(size_t index, std::string &steps) const
{
    if ( index >= _blocks.size() )
        return false;

    std::unique_ptr<FSMgr::FileHandle> file(OpenFile(_path, "rb"));
    std::string data;

    return file && ReadChunk(*file, _blocks[index].Start, _blocks[index].End, data) && data.size() >= 12 &&
           Unpacked(data.substr(8), steps);
}


// ViewReader

bool ViewReader::Rewind()
{
    return Seek(_recording.FirstStep());
}

bool ViewReader::Seek(uint32_t step)
{
    _view.clear();
    _changed.clear();
    _block.clear();
    _at = 0;

    const KeyframeAt *at = _recording.KeyframeAtOrBefore(step);
    State::Image view;

    if ( !at || at->Step != step || !ReadKeyframeChunk(_recording._path, *at, &view, NULL) )
    {
        _nextBlock = _recording._blocks.size();
        return false;
    }

    _step = step;

    for ( State::Object &obj : view.Objects )
    {
        uint64_t key = ObjectKey(obj.Kind, obj.Id);
        ObjectVersions &versions = _view[key];
        versions = ObjectVersions{obj.Data, obj.Data};
        _changed.emplace_back(key, &versions);
    }

    // The steps after it
    _nextBlock = 0;
    while ( _nextBlock < _recording._blocks.size() && _recording._blocks[_nextBlock].FirstStep <= step )
        _nextBlock++;

    return true;
}

bool ViewReader::AtEnd() const
{
    return _nextBlock >= _recording._blocks.size() && _at + 4 > _block.size();
}

bool ViewReader::Next()
{
    while ( _at + 4 > _block.size() )
    {
        if ( _nextBlock >= _recording._blocks.size() || _recording._blocks[_nextBlock].FirstStep != _step + 1 )
            return false;

        if ( !_recording.ReadBlock(_nextBlock, _block) )
        {
            _block.clear();
            _at = 0;
            return false;
        }

        _nextBlock++;
        _at = 0;

        // After a keyframe the guesses start over
        for ( auto &it : _view )
            it.second.Before = it.second.Data;
    }

    uint32_t stepSize;
    memcpy(&stepSize, _block.data() + _at, 4);

    if ( _at + 4 + stepSize > _block.size() )
        return false;

    const char *p = _block.data() + _at + 4;
    const char *end = p + stepSize;

    _changed.clear();
    if ( !ApplyChanges(_view, _recording._viewLayouts, p, end, &_changed) )
        return false;

    _at += 4 + stepSize;
    _step++;
    return true;
}

}
