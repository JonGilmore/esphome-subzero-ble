// All MessageBuffer implementations are inline in buffer.h. This .cpp is
// kept as a stub (rather than removed) so existing ESPHome build dirs —
// whose CMake source-list cache references this filename from earlier
// versions of this PR — continue to compile without requiring a manual
// "Clean Build Files" step. CMake's FILE(GLOB_RECURSE ...) caches the
// resolved source list across builds, so adding or removing .cpp files
// in an external_components dir requires a clean to be picked up.
#include "buffer.h"
