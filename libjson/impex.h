#ifdef _WIN32

#ifdef JSON_EXPORT
#ifndef JSON_API
#define JSON_API __declspec(dllexport)
#endif
#else
#ifndef JSON_API
#define JSON_API __declspec(dllimport)
#endif
#endif

#else

#define JSON_API 

#endif