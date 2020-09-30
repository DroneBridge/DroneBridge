# DroneBridge common library (libdb_common)

DroneBridge modules get compiled directly to the src of this folder.  
You can compile & install the library directly by running 
```C
cmake .
make
sudo make install
```

After that includes work as usually system wide like 
```C
#include <DroneBridge/db_common.h>
```