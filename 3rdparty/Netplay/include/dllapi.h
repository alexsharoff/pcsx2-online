#ifdef NETPLAY_EXPORTS
#define NETPLAY_API __declspec(dllexport)
#else
#define NETPLAY_API __declspec(dllimport)
#endif