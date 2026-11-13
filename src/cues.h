#pragma once

// Short start and stop sounds, so a dictation can be felt without looking at
// the bar. Generated once into the runtime directory and played by pw-play in
// the background, so a cue never delays the recording.
namespace Cues {

void prepare();
void start();
void stop();
void error();

}
