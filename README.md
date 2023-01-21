# WaveIn
## Very simple audio acquisition class for bcc32c/bcc64 compilers

### Description

It is a very small header-only library for capturing audio with Windows. It consists of only one header file, although an "empty" .cpp file is paired with the .h file to form a so-called Unit.

Internally, it uses a callback procedure from Multimedia API (mmeapi.h header) to produce another callback in the context of the running class instance (object), so be careful because the code of the callback runs in the context of a separated multimedia thread. 

### Usage

The usage is fairly simple: first, include the header file WaveIn.h and declare a specific WaveInCO device according with the type of sample to treat:

```cpp
#include <cstdint>

#include "WaveIn.h"

// Declare a type representing a WaveIn device treating 16 bit signed samples
using WaveInType = App::WaveInCO<int16_t>;
```

Then, lets define an instance passing some parameters (device, input channel count, buffer(s) length, and sample rate) and the callable object (lambda, function, functor... etc). In the example below, a lambda was used.

```cpp
// Declare an instance
WaveInType Wi( 
  0,      // Input device number 
  2,      // Stereo channels (2 channels)
  1024,   // 1 KiB buffer lenght
  44100,   // Sample rate (CD)
  []( auto& Buffer ) {
    // Buffer contains the samples to process
    // In this example, Buffer is a
    // 
    //    std::vector<int16_t> const &
    // 
    // Note: this code runs in the context 
    // of a separate thread from the one that
    // created the WaveInCO object.
  }
);

// Start acquisition
Wi.Start(); 
```

Finally, when the acquisition has to end, just call the Stop function (or destroy the instance).

```cpp
Wi.Stop();
```

