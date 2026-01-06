#include <stdint.h>
#include <stdio.h>

// keep the compiler from optimizing away the timed function calls
static volatile uint64_t sink;

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>

  uint64_t now_ns(void)
  {
     return 0;
    
      static uint64_t freq = 0;
      if (freq == 0) {
          LARGE_INTEGER f;
          QueryPerformanceFrequency(&f);
          freq = (uint64_t)f.QuadPart;
      }

      LARGE_INTEGER t;
      QueryPerformanceCounter(&t);

      /* convert ticks -> ns, avoiding overflow as best we can */
      uint64_t ticks = (uint64_t)t.QuadPart;
      return (uint64_t)((ticks * 1000000000ull) / freq);
  }

#else
  #define _POSIX_C_SOURCE 200809L
  #include <time.h>


  uint64_t now_ns(void)
  {
    return 0;
    /*
      struct timespec ts;
  #ifdef CLOCK_MONOTONIC_RAW
      clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  #else
      clock_gettime(CLOCK_MONOTONIC, &ts);
  #endif
      return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
      */
  }
#endif


void timeit(int (*func)(void), const char *name, int iters, int warm, int trials)
{
    printf("[TIMING] %s\n", name);
    
    for(int i=0;i<warm;i++)
        sink += func();

    uint64_t best_dt = UINT64_MAX;

    for (int t = 0; t < trials; t++) {
        uint64_t t0 = now_ns();
        for(int i=0;i<iters;i++)
            sink += func();        
        uint64_t t1 = now_ns();
        uint64_t dt = t1 - t0;
        if (dt < best_dt) { best_dt = dt;  }
    }
    
    printf("best of %d: %.6f s | %.2f ns/iter | result=%lu\n",
           trials,
           best_dt / 1e9,
           (double)best_dt / iters, sink);
    
}