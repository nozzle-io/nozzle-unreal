#include "nozzle_unreal_native_bridge.h"

// The bridge implementation is intentionally header-inline so both the CMake
// compile-check and the Unreal module can depend on the same native-device
// creation/publish/copy seam. Keeping logic here would let CI validate a path
// that the Unreal component does not actually call.
