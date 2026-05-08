#include "sound/music.hpp"

namespace sound {

MusicInfo::MusicInfo() :
    mTrackName(""),
    mJapaneseName(""),
    mSampleRate(48000),
    mLoopStart(0),
    mLoopEnd(0),
    mDefaultTempoMeter(),
    mTempoMeters(),
    mBeats(),
    mMeasures(),
    mPointMarkers(),
    mRangeMarkers() {}

} // namespace sound