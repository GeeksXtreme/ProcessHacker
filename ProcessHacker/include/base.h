#ifndef BASE_H
#define BASE_H

#ifndef UNICODE
#define UNICODE
#endif

#include <ntwin.h>
#include <intrin.h>
#include <wchar.h>

// nonstandard extension used : nameless struct/union
#pragma warning(disable: 4201)
// nonstandard extension used : bit field types other than int
#pragma warning(disable: 4214)
// 'function': was declared deprecated
#pragma warning(disable: 4996)

#define PTR_ADD_OFFSET(Pointer, Offset) ((PVOID)((ULONG_PTR)(Pointer) + (ULONG_PTR)(Offset)))
#define PTR_SUB_OFFSET(Pointer, Offset) ((PVOID)((ULONG_PTR)(Pointer) - (ULONG_PTR)(Offset)))
#define REBASE_ADDRESS(Pointer, OldBase, NewBase) \
    ((PVOID)((ULONG_PTR)(Pointer) - (ULONG_PTR)(OldBase) + (ULONG_PTR)(NewBase)))

#define WCHAR_LONG_TO_SHORT(Long) (((Long) & 0xff) | (((Long) & 0xff0000) >> 16))

#define PH_TIMEOUT_TO_MS ((LONGLONG)1 * 10 * 1000)
#define PH_TIMEOUT_TO_SEC (PH_TIMEOUT_TO_MS * 1000)

#define PhRaiseStatus(Status) RaiseException(Status, 0, 0, NULL)

#define PH_INT32_STR_LEN 12
#define PH_INT32_STR_LEN_1 (PH_INT32_STR_LEN + 1)

#define PH_INT64_STR_LEN 50
#define PH_INT64_STR_LEN_1 (PH_INT64_STR_LEN + 1)

#define PH_PTR_STR_LEN 24
#define PH_PTR_STR_LEN_1 (PH_PTR_STR_LEN + 1)

#define STR_EQUAL(Str1, Str2) (strcmp(Str1, Str2) == 0)
#define WSTR_EQUAL(Str1, Str2) (wcscmp(Str1, Str2) == 0)

FORCEINLINE PVOID PhGetProcAddress(
    __in PWSTR LibraryName,
    __in PSTR ProcName
    )
{
    return GetProcAddress(GetModuleHandle(LibraryName), ProcName);
}

FORCEINLINE VOID PhPrintInt32(
    __out PWSTR Destination,
    __in LONG Int32
    )
{
    _snwprintf(Destination, PH_INT32_STR_LEN, L"%d", Int32);
}

FORCEINLINE VOID PhPrintUInt32(
    __out PWSTR Destination,
    __in ULONG UInt32
    )
{
    _snwprintf(Destination, PH_INT32_STR_LEN, L"%u", UInt32);
}

FORCEINLINE VOID PhPrintInt64(
    __out PWSTR Destination,
    __in LONG64 Int64
    )
{
    _snwprintf(Destination, PH_INT64_STR_LEN, L"%I64d", Int64);
}

FORCEINLINE VOID PhPrintUInt64(
    __out PWSTR Destination,
    __in ULONG64 UInt64
    )
{
    _snwprintf(Destination, PH_INT64_STR_LEN, L"%I64u", UInt64);
}

FORCEINLINE VOID PhPrintPointer(
    __out PWSTR Destination,
    __in PVOID Pointer
    )
{
    _snwprintf(Destination, PH_PTR_STR_LEN, L"0x%Ix", Pointer);
}

#ifdef _M_IX86

FORCEINLINE PVOID _InterlockedCompareExchangePointer(
    __inout PVOID volatile *Destination,
    __in PVOID Exchange,
    __in PVOID Comparand
    )
{
    return (PVOID)_InterlockedCompareExchange(
        (PLONG_PTR)Destination,
        (LONG_PTR)Exchange,
        (LONG_PTR)Comparand
        );
}

FORCEINLINE PVOID _InterlockedExchangePointer(
    __inout PVOID volatile *Destination,
    __in PVOID Exchange
    )
{
    return (PVOID)_InterlockedExchange(
        (PLONG_PTR)Destination,
        (LONG_PTR)Exchange
        );
}

#endif

#endif