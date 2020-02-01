#include <core/core.h>

#define TR2() \
{ \
    char buf[1024]; \
    unsigned int size=1024; \
    int status; \
    const char* str = abi::__cxa_demangle (typeid(*this).name(), buf, &size, &status); \
    printf("%s::%s() : %d\n", str, __FUNCTION__, __LINE__); \
}
#define TR3()   //printf("%s:%d - %s\n",  __FUNCTION__, __LINE__, __FILE__);
#define TR4()   //printf("%s : %d\n",     __PRETTY_FUNCTION__, __LINE__);

#define TR()  printf("--- %s:%d\n",       __FUNCTION__, __LINE__);
//#define TR()        TRACE(Trace::Information, (_T("---")));                     // WTF: Cannot use Information in function (i.e. not in a class)

#include <sys/time.h>
#include <chrono>
#include <string>

//#define TR() { struct timeval tv; gettimeofday(&tv, NULL); printf("===>%10lu %s() %s:%d\n", tv.tv_sec+tv.tv_usec, __func__, __FILE__, __LINE__); }

using namespace std::chrono;
class TraceExit {
    public:
    TraceExit(std::string method, int line)
        : _method(method)
        , _line(line)
    {
        _start = now();

        printf("--> %llu: %s:%d Enter\n", now(), _method.c_str(), _line );
    }
    ~TraceExit()
    {
        uint64_t end = now();
        uint32_t diff = end - _start;

        printf("<-- %llu: %s:%d Exit Elapsed time %u microsec\n", now(), _method.c_str(), _line, diff);
    }

    static uint64_t now() {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return ((uint64_t) tv.tv_sec * 1000000L + tv.tv_usec );
    }

    private:
    std::string _method;
    uint32_t _line;
    uint64_t _start;
};
#define TRACEEXIT() TraceExit te(__FUNCTION__, __LINE__)

