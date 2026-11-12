#ifndef CLOSED_REGISTRY_API_H
#define CLOSED_REGISTRY_API_H
typedef struct RegistryLeft {} enum { one } const RegistryLeft;
typedef struct RegistryRight {} enum { one, two } const RegistryRight;
int registry_combine<RegistryLeft *a, RegistryRight *b>(int x) = 0;
#endif
