#pragma once

namespace mixworks
{
class Engine;
// Shared playback/export end, including cascaded track and mixer-lane inserts.
double effectAudibleEnd(const Engine&);
}
