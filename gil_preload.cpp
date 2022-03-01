
#include "gil_preload.hpp"

#include <dlfcn.h>
#include <nvtx3/nvtx3.hpp>
#include <pthread.h>

namespace // begin anonymous
{
    // Flag set by the Cython/Python function immediately after acquiring the GIL
    // to indicate that the following call to pthread_mutex_unlock is with a pointer
    // to the GIL mutex
    static bool initialized = false;

    struct GILDomain{ static constexpr char const* name{"Python GIL"}; };

    nvtx3::event_attributes gil_waiting_attr;
    nvtx3::event_attributes gil_holding_attr;

    using lock_func_t = int (*)(pthread_mutex_t*);
    using unlock_func_t = int(*)(pthread_mutex_t*);

    static lock_func_t real_mutex_lock;
    static unlock_func_t real_mutex_unlock;
    static pthread_mutex_t *GIL_acq_mutex = nullptr;

    bool is_gil(pthread_mutex_t const *const m)
    {
        return m == GIL_acq_mutex;
    }

    void push_if_gil(pthread_mutex_t const *mutex, nvtx3::event_attributes const &attr)
    {
        if (is_gil(mutex))
        {
            nvtxDomainRangePushEx(nvtx3::domain::get<GILDomain>(), attr.get());
        }
    }

    void pop_if_gil(pthread_mutex_t const *mutex)
    {
        if (is_gil(mutex))
        {
            nvtxDomainRangePop(nvtx3::domain::get<GILDomain>());
        }
    }

} // end anonymous

// To be called from Cython/Python after acquiring the GIL
void set_initialized()
{
    // Can't call NVTX during library load, so put them in the explicit init function
    static nvtx3::registered_string<GILDomain> gil_waiting_msg{"Waiting for GIL"};
    static nvtx3::registered_string<GILDomain> gil_holding_msg{"Holding GIL"};
    gil_waiting_attr = nvtx3::event_attributes{gil_waiting_msg, nvtx3::rgb{255, 0, 0}};
    gil_holding_attr = nvtx3::event_attributes{gil_waiting_msg, nvtx3::rgb{0, 255, 0}};

    initialized = true;
}

void __attribute__((constructor)) init();
void init()
{
    real_mutex_lock = (lock_func_t)dlsym(RTLD_NEXT, "pthread_mutex_lock");
    real_mutex_unlock = (unlock_func_t)dlsym(RTLD_NEXT, "pthread_mutex_unlock");
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    push_if_gil(mutex, gil_waiting_attr); // begin GIL waiting range

    int const r = real_mutex_lock(mutex);

    pop_if_gil(mutex); // end GIL waiting range

    push_if_gil(mutex, gil_holding_attr); // begin GIL holding range

    return r;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    if (not initialized)
    {
        // Cython will call set_initialized() immediately after acquiring the GIL.
        // We know that the next call to pthread_mutex_unlock will then be with a
        // pointer to the GIL mutex. So we just store every mutex we see until
        // set_initialized is called
        GIL_acq_mutex = mutex;
    }

    int ret = real_mutex_unlock(mutex);

    pop_if_gil(mutex); // end GIL holding range

    return ret;
}
