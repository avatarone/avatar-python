#ifndef __PROXY_MEMORY_OBJECT_H
#define __PROXY_MEMORY_OBJECT_H

#include <stdint.h>
#include  "llvm/Support/MemoryObject.h"

typedef unsigned (*ReadCodeMemoryFunc)(void *opaque, uint64_t address, unsigned size, uint8_t *buffer);

class ProxyMemoryObject : public llvm::MemoryObject {
    private:
         void *opaque;
         ReadCodeMemoryFunc read_func;
         size_t size;
         uint64_t base;
    public:
        ProxyMemoryObject(ReadCodeMemoryFunc read_func, void * opaque, uint64_t base = 0, size_t size = 0xFFFFFFFF);

        virtual uint64_t getBase() const;
        virtual uint64_t getExtent() const;
        virtual int readByte(uint64_t addr, uint8_t * byte) const;
        virtual int readBytes(uint64_t addr, uint64_t size, uint8_t * byte) const;
        virtual ~ProxyMemoryObject();
};

#endif /*  __PROXY_MEMORY_OBJECT_H */
