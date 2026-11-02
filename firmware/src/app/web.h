// The macro editor: a web page served by the Tab5 on its Wi-Fi address
// (and http://tab5-multitool.local). Saving goes through macrosSave(), so a
// bad layout is refused with the reason and nothing on the Tab5 changes.
//
// It only edits layouts; nothing here can make the Tab5 send keys.
#pragma once

namespace mt {

void webBegin();  // safe to call before Wi-Fi is up; starts once it is
void webTick();   // from loop()
bool webRunning();

}  // namespace mt
