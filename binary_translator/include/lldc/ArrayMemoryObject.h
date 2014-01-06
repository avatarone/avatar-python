#ifndef __ARRAY_MEMORY_OBJECT_H
#define __ARRAY_MEMORY_OBJECT_H

#include  "llvm/Support/MemoryObject.h"

class ArrayMemoryObject : public llvm::MemoryObject {
    private:
         uint8_t * data;
         size_t size;
         uint64_t base;
    public:
        ArrayMemoryObject(uint64_t base, size_t size);

        virtual uint64_t getBase() const;
        virtual uint64_t getExtent() const;
        virtual int readByte(uint64_t addr, uint8_t * byte) const;
        virtual ~ArrayMemoryObject();
        uint8_t * getArray();
};

#endif /*  __ARRAY_MEMORY_OBJECT_H */
