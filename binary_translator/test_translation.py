#!/usr/bin/env python3

from binary_translator import instrument_memory_access

memory_accesses = []

def read_code_memory(address, size):
    memory_accesses.append((address, size))
#    print("Reading code memory 0x%08x[%d]" % (address, size))
    if address >= 0x1146 and address < 0x1218:
        address -= 0x1146
        return bytes([160, 137, 114, 73, 8, 67, 160, 129, 96, 139, 255, 34, 
            65, 50, 16, 67, 96, 131, 32, 139, 240, 33, 8, 64, 110, 73, 
            64, 24, 32, 131, 160, 139, 192, 11, 192, 3, 125, 48, 160, 
            131, 96, 139, 48, 67, 96, 131, 96, 139, 176, 67, 96, 131, 
            160, 139, 1, 33, 201, 3, 8, 67, 160, 131, 160, 139, 64, 4, 
            64, 12, 160, 131, 0, 32, 99, 73, 64, 28, 136, 66, 252, 211, 
            224, 137, 97, 73, 128, 11, 128, 3, 64, 24, 224, 129, 160, 
            136, 95, 73, 192, 11, 192, 3, 64, 24, 160, 128, 224, 136, 
            93, 73, 8, 64, 255, 48, 192, 28, 224, 128, 32, 137, 91, 73, 
            192, 11, 192, 3, 64, 24, 32, 129, 96, 137, 89, 73, 192, 11, 
            192, 3, 64, 24, 96, 129, 32, 143, 87, 73, 192, 11, 192, 3, 
            64, 24, 32, 135, 96, 139, 144, 67, 96, 131, 160, 137, 64, 
            10, 64, 2, 32, 48, 160, 129, 82, 72, 1, 104, 73, 8, 73, 0, 
            1, 96, 0, 34, 66, 96, 79, 73, 129, 96, 79, 73, 1, 98, 79, 
            73, 65, 98, 79, 73, 1, 98, 1, 104, 57, 67, 1, 96, 1, 104, 
            201, 7, 252, 208][address : address + size])
    elif address >=  0x128c and address < 0x1372:
        address -= 0x128c
        return bytes([0, 0, 13, 64, 0, 8, 10, 64, 64, 144, 1, 64, 0, 
            16, 13, 64, 200, 39, 0, 4, 128, 3, 10, 64, 204, 38, 0, 0, 
            132, 156, 5, 6, 0, 0, 0, 0, 0, 47, 4, 6, 128, 39, 1, 64, 
            0, 42, 1, 64, 192, 112, 3, 6, 192, 29, 0, 0, 156, 3, 0, 
            4, 80, 195, 5, 6, 172, 191, 5, 6, 0, 48, 13, 64, 64, 0, 
            16, 0, 176, 28, 0, 0, 188, 192, 5, 6, 7, 5, 0, 0, 150, 
            250, 255, 255, 148, 0, 0, 4, 136, 3, 0, 4, 13, 10, 82, 
            115, 116, 32, 48, 120, 0, 0, 0, 0, 77, 0, 0, 0, 0, 43, 
            1, 64, 0, 144, 1, 64, 13, 240, 173, 222, 237, 254, 173, 
            222, 200, 3, 0, 4, 255, 1, 0, 0, 5, 57, 0, 0, 160, 134, 1, 
            0, 110, 46, 0, 0, 198, 122, 0, 0, 1, 248, 255, 255, 22, 13, 
            0, 0, 66, 9, 0, 0, 10, 25, 0, 0, 0, 160, 13, 64, 0, 0, 
            254, 3, 255, 255, 255, 127, 112, 71, 112, 71, 204, 8, 0, 
            128, 0, 71, 1, 64, 160, 0, 7, 64, 10, 43, 0, 0, 11, 51, 
            0, 0, 12, 45, 0, 0, 54, 53, 0, 0, 170, 114, 0, 0, 69, 77, 
            0, 0, 109, 113, 0, 0, 112, 71][address : address + size])
    else:  
        print("Invalid address requested: 0x%x" % address)

data = instrument_memory_access(
            architecture = "thumb", 
            entry_point = 0x1146, 
            valid_pc_ranges = [(0x1146, 0x1218)] , 
            generated_code_address = 0x2000, 
            get_code_callback = read_code_memory, 
            opts = {"print_ir": "true", "debug": "true", "instrument_memory_access": "true"})                                
f = open("code.bin", "wb")
f.write(data["generated_code"])
f.close()

def merge_ranges(ranges):
    regions = []
    
    for r in ranges:
        range_done = False
        for region in regions:
            if r[0] >= region[0] and r[0] <= region[1]:
                if r[1] > region[1]:
                    region[1] = r[1]
                range_done = True
                break
            elif r[1] >= region[0] and r[1] <= region[1]:
                if r[0] < region[0]:
                    region[0] = r[0]
                range_done = True
                break
                
        if not range_done:
            regions.append(r)
            
    if regions == ranges:
        return regions
    else:
        return merge_ranges(regions)
        
bla = [[x, x + y] for (x, y) in memory_accesses]
print([(hex(x), hex(y)) for [x, y] in merge_ranges(bla)])

#print(data)
#print("Size: %d" % len(data['generated_code']))
#print(memory_accesses)
