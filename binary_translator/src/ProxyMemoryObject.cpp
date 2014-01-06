#include "lldc/ProxyMemoryObject.h"


ProxyMemoryObject::ProxyMemoryObject(ReadCodeMemoryFunc read_func, void * opaque, uint64_t base, size_t size) 
    : opaque(opaque),
      read_func(read_func),
      size(size),
      base(base)
{
}

uint64_t ProxyMemoryObject::getBase() const 
{
    return this->base;
}

uint64_t ProxyMemoryObject::getExtent() const 
{
    return this->size;
}
         
int ProxyMemoryObject::readByte(uint64_t addr, uint8_t * byte) const 
{
    if (this->read_func(this->opaque, addr, 1, byte) == 1)
        return 0;
    else
        return -1;
}

int ProxyMemoryObject::readBytes(uint64_t addr, uint64_t size, uint8_t * byte) const 
{
    if (this->read_func(this->opaque, addr, size, byte) == size)
        return 0;
    else
        return -1;
}

ProxyMemoryObject::~ProxyMemoryObject() 
{
}