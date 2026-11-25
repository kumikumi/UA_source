#ifndef WORLD_TUTORIALHINTS_H_INCLUDED
#define WORLD_TUTORIALHINTS_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <vector>

class NC_STACK_ypaworld;

namespace World
{
namespace State { class Visitor; }

// The spoken hints of the training missions. A level's event_loop, 1 to 3,
// picks a list of hints. Each update goes down the list to the first hint
// with something to say, which plays its next message, and plays again
// after its time for as long as it stays the first.
class TutorialHints
{
public:
    // The hints of an event loop; an event loop without hints has none
    void Start(int eventLoopId);
    void Stop();

    void Update(NC_STACK_ypaworld *world);

    // How far the hints have got: see World::State
    void VisitState(State::Visitor &v);

protected:
    enum HintType
    {
        // Every time its time has passed since it was first reached or
        // last played, unless done. Later hints go on in between.
        HINT_REPEAT,

        // Whenever reached until done, holding back the later hints
        HINT_UNTIL_DONE,

        // At once, then every time its time has passed, unless done
        HINT_NOW_AND_REPEAT,

        // Holds back the later hints for its time, then plays once unless
        // done
        HINT_AFTER_PAUSE,

        // As HINT_AFTER_PAUSE, and then offers to leave the training
        // mission
        HINT_FINISH,
    };

    enum Result
    {
        RESULT_PLAY, // Plays, and the later hints wait
        RESULT_WAIT, // The later hints wait
        RESULT_SKIP, // On to the next hint
    };

    struct Hint
    {
        HintType Type;
        int32_t Time; // Milliseconds
        bool (*IsDone)(NC_STACK_ypaworld *world); // NULL: never done
        std::vector<int32_t> Messages; // Voice message ids; 0 says nothing

        bool Reached = false;
        int32_t ReachedTime = 0; // When reached, or last repeated
        bool Played = false; // Has had its one turn, or its first
        size_t NextMessage = 0;
    };

    Result Consider(NC_STACK_ypaworld *world, Hint &hint);
    void Play(NC_STACK_ypaworld *world, size_t hintId);

    std::vector<Hint> _hints;
    int _lastPlayedHint = -1;
    int32_t _lastPlayTime = 0;
};

}

#endif
