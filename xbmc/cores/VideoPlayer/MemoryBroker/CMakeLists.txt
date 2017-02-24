set(SOURCES
MemoryBroker.cpp
)

set(HEADERS
MemoryBroker.h
MemoryProvider.h
)

if(CORE_SYSTEM_NAME STREQUAL rbpi)
  list(APPEND SOURCES MemoryProviderPi.cpp)
  list(APPEND HEADERS MemoryProviderPi.h)
else()
  list(APPEND SOURCES MemoryProviderMalloc.cpp)
  list(APPEND HEADERS MemoryProviderPi.h)
endif()


core_add_library(memorybroker)
