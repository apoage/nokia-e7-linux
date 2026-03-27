// Sized delete — GCC 12+ emits _ZdlPvj instead of _ZdlPv
void operator delete(void* p, unsigned int) throw() { ::operator delete(p); }

extern "C" {
void* __cxa_begin_catch(void* p) { return p; }
void __cxa_end_catch() {}
void __cxa_end_cleanup() {}

// XLeaveException typeinfo
struct fake_typeinfo {
    void* vtable_ptr;
    const char* name;
};
// Provide the typeinfo symbol as a weak definition  
static const char _ZTS15XLeaveException[] = "15XLeaveException";
extern const fake_typeinfo _ZTI15XLeaveException __attribute__((weak));
const fake_typeinfo _ZTI15XLeaveException = { 0, _ZTS15XLeaveException };

// TCppRTExceptionsGlobals constructor
void _ZN23TCppRTExceptionsGlobalsC1Ev(void*) {}
}
