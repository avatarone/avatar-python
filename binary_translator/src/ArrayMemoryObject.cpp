#include "lldc/ArrayMemoryObject.h"


ArrayMemoryObject::ArrayMemoryObject(uint64_t base, size_t size) {
    this->data = new uint8_t[size];
    this->size = size;
    this->base = base;
}

uint64_t ArrayMemoryObject::getBase() const 
{
    return this->base;
}

uint64_t ArrayMemoryObject::getExtent() const 
{
    return this->size;
}
         
int ArrayMemoryObject::readByte(uint64_t addr, uint8_t * byte) const 
{
    uint64_t offset = addr - this->base;
    if (offset > this->size)
        return -1;
    *byte = this->data[offset];
    return 0;
}

ArrayMemoryObject::~ArrayMemoryObject() 
{
    if (this->data) delete[] this->data;
}

uint8_t * ArrayMemoryObject::getArray() 
{
    return this->data;
}