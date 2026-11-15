#include <cstdlib>
#include <new>

namespace
{
	bool failNextAllocation = false;
	unsigned allocationFaultHits = 0;
}

void ArmInputAllocationFailure() noexcept
{
	failNextAllocation = true;
}

unsigned GetInputAllocationFaultHits() noexcept
{
	return allocationFaultHits;
}

_Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(std::size_t size)
{
	if (failNextAllocation)
	{
		failNextAllocation = false;
		++allocationFaultHits;
		throw std::bad_alloc();
	}
	if (void* memory = std::malloc(size == 0 ? 1 : size))
	{
		return memory;
	}
	throw std::bad_alloc();
}

void operator delete(void* memory) noexcept
{
	std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
	std::free(memory);
}
