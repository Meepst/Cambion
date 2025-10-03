#pragma once

#include "common.h"
#include "primitives.h"

#include "fast_obj.h"


bool loadModel(Allocator& allocator, DynArray<Vertex>& vertices, DynArray<uint32_t>& indices, const char* path);
