/*
 * Process Hacker - 
 *   internal object manager
 * 
 * Copyright (C) 2009-2010 wj32
 * 
 * This file is part of Process Hacker.
 * 
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

// This code was initially ported from KProcessHacker.

#include <refp.h>

/** The type object type. */
PPH_OBJECT_TYPE PhObjectTypeObject = NULL;

/** Whether the object manager is destroying all objects. */
BOOLEAN PhObjectDeinitializing = FALSE;
/** The next object to delete. */
PPH_OBJECT_HEADER PhObjectNextToFree = NULL;

static ULONG PhpAutoPoolTlsIndex;

/**
 * Initializes the object manager module.
 */
NTSTATUS PhInitializeRef()
{
    NTSTATUS status = STATUS_SUCCESS;
    
    // Create the fundamental object type.
    status = PhCreateObjectType(
        &PhObjectTypeObject,
        0,
        NULL
        );
    
    if (!NT_SUCCESS(status))
        return status;
    
    // Now that the fundamental object type exists, fix it up.
    PhObjectToObjectHeader(PhObjectTypeObject)->Type = PhObjectTypeObject;
    PhObjectTypeObject->NumberOfObjects = 1;

    // Reserve a slot for the auto pool.
    PhpAutoPoolTlsIndex = TlsAlloc();

    if (PhpAutoPoolTlsIndex == TLS_OUT_OF_INDEXES)
        return STATUS_INSUFFICIENT_RESOURCES;
    
    return status;
}

/**
 * Allocates a object.
 * 
 * \param Object A variable which receives a pointer to the newly allocated object.
 * \param ObjectSize The size of the object.
 * \param Flags A combination of flags specifying how the object is to be allocated.
 * \li \c PHOBJ_RAISE_ON_FAIL An exception will be raised if the object cannot be 
 * allocated.
 * \param ObjectType The type of the object.
 * \param AdditionalReferences The number of references to add to the object. The 
 * object will initially have a reference count of 1 + AdditionalReferences.
 */
NTSTATUS PhCreateObject(
    __out PVOID *Object,
    __in SIZE_T ObjectSize,
    __in ULONG Flags,
    __in_opt PPH_OBJECT_TYPE ObjectType,
    __in_opt LONG AdditionalReferences
    )
{
    PPH_OBJECT_HEADER objectHeader;
    
    /* Check the flags. */
    if ((Flags & PHOBJ_VALID_FLAGS) != Flags) /* Valid flag mask */
        return STATUS_INVALID_PARAMETER_3;
    /* The object type is only optional if the fundamental object type 
     * hasn't been created. */
    if (!ObjectType && PhObjectTypeObject)
        return STATUS_INVALID_PARAMETER_4;
    /* Make sure the additional reference count isn't negative. */
    if (AdditionalReferences < 0)
        return STATUS_INVALID_PARAMETER_5;
    
    /* Allocate storage for the object. Note that this includes 
     * the object header followed by the object body. */
    objectHeader = PhpAllocateObject(ObjectSize);
    
    if (!objectHeader)
    {
        if (Flags & PHOBJ_RAISE_ON_FAIL)
            PhRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
        else
            return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    /* Object type statistics. */
    if (ObjectType)
    {
        _InterlockedIncrement((PLONG)&ObjectType->NumberOfObjects);
    }
    
    /* Initialize the object header. */
    objectHeader->RefCount = 1 + AdditionalReferences;
    objectHeader->Flags = Flags;
    objectHeader->Size = ObjectSize;
    objectHeader->Type = ObjectType;
    
    /* Pass a pointer to the object body back to the caller. */
    *Object = PhObjectHeaderToObject(objectHeader);
    
    return STATUS_SUCCESS;
}

/**
 * Creates an object type.
 *
 * \param ObjectType A variable which receives a pointer to the newly 
 * created object type.
 * \param Flags A combination of flags affecting the behaviour of the 
 * object type.
 * \param DeleteProcedure A callback function that is executed when 
 * an object of this type is about to be freed (i.e. when its 
 * reference count is 0).
 *
 * \remarks Do not reference or dereference the object type once it 
 * is created.
 */
NTSTATUS PhCreateObjectType(
    __out PPH_OBJECT_TYPE *ObjectType,
    __in ULONG Flags,
    __in_opt PPH_TYPE_DELETE_PROCEDURE DeleteProcedure
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PPH_OBJECT_TYPE objectType;
    
    /* Check the flags. */
    if ((Flags & PHOBJTYPE_VALID_FLAGS) != Flags) /* Valid flag mask */
        return STATUS_INVALID_PARAMETER_3;
    
    /* Create the type object. */
    status = PhCreateObject(
        &objectType,
        sizeof(PH_OBJECT_TYPE),
        0,
        PhObjectTypeObject,
        0
        );
    
    if (!NT_SUCCESS(status))
        return status;
    
    /* Initialize the type object. */
    objectType->Flags = Flags;
    objectType->DeleteProcedure = DeleteProcedure;
    objectType->NumberOfObjects = 0;
    
    *ObjectType = objectType;
    
    return status;
}

/**
 * Dereferences the specified object.
 * The object will be freed if its reference count reaches 0.
 * 
 * \param Object A pointer to the object to dereference.
 * 
 * \return TRUE if the object was freed, otherwise FALSE.
 */
VOID PhDereferenceObject(
    __in PVOID Object
    )
{
    PPH_OBJECT_HEADER objectHeader;
    LONG newRefCount;
    
    objectHeader = PhObjectToObjectHeader(Object);
    /* Decrement the reference count. */
    newRefCount = _InterlockedDecrement(&objectHeader->RefCount);
    
    /* Free the object if it has 0 references. */
    if (newRefCount == 0)
    {
        PhpFreeObject(objectHeader);
    }
}

/**
 * Dereferences the specified object.
 * The object will be freed in a worker thread if its reference count 
 * reaches 0.
 * 
 * \param Object A pointer to the object to dereference.
 * 
 * \return TRUE if the object was freed, otherwise FALSE.
 */
BOOLEAN PhDereferenceObjectDeferDelete(
    __in PVOID Object
    )
{
    return PhDereferenceObjectEx(Object, 1, TRUE) == 0;
}

/**
 * Dereferences the specified object.
 * The object will be freed if its reference count reaches 0.
 * 
 * \param Object A pointer to the object to dereference.
 * \param RefCount The number of references to remove.
 * \param DeferDelete Whether to defer deletion of the object.
 * 
 * \return The new reference count of the object.
 */
LONG PhDereferenceObjectEx(
    __in PVOID Object,
    __in LONG RefCount,
    __in BOOLEAN DeferDelete
    )
{
    PPH_OBJECT_HEADER objectHeader;
    LONG oldRefCount;
    LONG newRefCount;
    
    /* Make sure we're not subtracting a negative reference count. */
    if (RefCount < 0)
        PhRaiseStatus(STATUS_INVALID_PARAMETER_2);
    
    objectHeader = PhObjectToObjectHeader(Object);
    
    /* Decrease the reference count. */
    oldRefCount = _InterlockedExchangeAdd(&objectHeader->RefCount, -RefCount);
    newRefCount = oldRefCount - RefCount;
    
    /* Free the object if it has 0 references. */
    if (newRefCount == 0)
    {
        if (DeferDelete)
        {
            PhpDeferDeleteObject(objectHeader);
        }
        else
        {
            /* Free the object. */
            PhpFreeObject(objectHeader);
        }
    }
    else if (newRefCount < 0)
    {
        PhRaiseStatus(STATUS_INVALID_PARAMETER);
    }
    
    return newRefCount;
}

/** 
 * Gets an object's type.
 *
 * \param Object A pointer to an object.
 *
 * \return A pointer to a type object.
 */
PPH_OBJECT_TYPE PhGetObjectType(
    __in PVOID Object
    )
{
    return PhObjectToObjectHeader(Object)->Type;
}

/**
 * References the specified object.
 * 
 * \param Object A pointer to the object to reference.
 */
VOID PhReferenceObject(
    __in PVOID Object
    )
{
    PPH_OBJECT_HEADER objectHeader;
    
    objectHeader = PhObjectToObjectHeader(Object);
    /* Increment the reference count. */
    _InterlockedIncrement(&objectHeader->RefCount);
}

/** 
 * References the specified object.
 * 
 * \param Object A pointer to the object to reference.
 * \param RefCount The number of references to add.
 * 
 * \return The new reference count of the object.
 */
LONG PhReferenceObjectEx(
    __in PVOID Object,
    __in LONG RefCount
    )
{
    PPH_OBJECT_HEADER objectHeader;
    LONG oldRefCount;
    
    /* Make sure we're not adding a negative reference count. */
    if (RefCount < 0)
        PhRaiseStatus(STATUS_INVALID_PARAMETER_2);
    
    objectHeader = PhObjectToObjectHeader(Object);
    /* Increase the reference count. */
    oldRefCount = _InterlockedExchangeAdd(&objectHeader->RefCount, RefCount);
    
    return oldRefCount + RefCount;
}

/**
 * Attempts to reference an object and fails if it is being 
 * destroyed.
 * 
 * \param Object The object to reference if it is not being deleted.
 * 
 * \return TRUE if the object was referenced, FALSE if 
 * it was being deleted and was not referenced.
 * 
 * \remarks
 * This function is useful if a reference to an object is 
 * held, protected by a mutex, and the delete procedure of 
 * the object's type attempts to acquire the mutex. If this 
 * function is called while the mutex is owned, you can 
 * avoid referencing an object that is being destroyed.
 */
BOOLEAN PhReferenceObjectSafe(
    __in PVOID Object
    )
{
    PPH_OBJECT_HEADER objectHeader;
    BOOLEAN result;
    
    objectHeader = PhObjectToObjectHeader(Object);
    /* Increase the reference count only if it isn't 0 (atomically). */
    result = PhpInterlockedIncrementSafe(&objectHeader->RefCount);
    
    return result;
}

/**
 * Allocates storage for an object.
 * 
 * \param ObjectSize The size of the object, excluding the header.
 */
PPH_OBJECT_HEADER PhpAllocateObject(
    __in SIZE_T ObjectSize
    )
{
    return PhAllocate(PhpAddObjectHeaderSize(ObjectSize));
}

/**
 * Queues an object for deletion.
 *
 * \param ObjectHeader A pointer to the object header of the object 
 * to delete.
 */
VOID PhpDeferDeleteObject(
    __in PPH_OBJECT_HEADER ObjectHeader
    )
{
    PPH_OBJECT_HEADER nextToFree;
    
    /* Add the object to the list while saving the old value, atomically.
     * Note that it is first-in, last-out.
     */
    while (TRUE)
    {
        nextToFree = PhObjectNextToFree;
        ObjectHeader->NextToFree = nextToFree;
        
        /* Attempt to set the global next-to-free variable. */
        if (_InterlockedCompareExchangePointer(
            &PhObjectNextToFree,
            ObjectHeader,
            nextToFree
            ) == nextToFree)
        {
            /* Success. */
            break;
        }
        
        /* Someone else changed the next-to-free variable. 
         * Go back and try again.
         */
    }
    
    /* Was the to-free list empty before? If so, we need to queue 
     * a work item.
     */
    if (!nextToFree)
    {
        PhQueueGlobalWorkQueueItem(PhpDeferDeleteObjectRoutine, NULL);
    }
}

/** 
 * Removes and frees objects from the to-free list.
 */
NTSTATUS PhpDeferDeleteObjectRoutine(
    __in PVOID Parameter
    )
{
    PPH_OBJECT_HEADER objectHeader = NULL;
    
    while (TRUE)
    {
        /* Get the next object to free while replacing the global variable with 
         * what we needed to free next.
         */
        objectHeader = _InterlockedExchangePointer(&PhObjectNextToFree, objectHeader);

        /* If we have an object to free, free it and move on to the 
         * next object. Otherwise, stop.
         */
        if (objectHeader)
        {
            PhpFreeObject(objectHeader);
            objectHeader = objectHeader->NextToFree;
        }
        else
        {
            break;
        }
    }

    return STATUS_SUCCESS;
}

/**
 * Calls the delete procedure for an object and frees its 
 * allocated storage.
 * 
 * \param ObjectHeader A pointer to the object header of an allocated object.
 */
VOID PhpFreeObject(
    __in PPH_OBJECT_HEADER ObjectHeader
    )
{
    /* Object type statistics. */
    _InterlockedDecrement(&ObjectHeader->Type->NumberOfObjects);
    
    /* Call the delete procedure if we have one. */
    if (ObjectHeader->Type->DeleteProcedure)
    {
        ObjectHeader->Type->DeleteProcedure(
            PhObjectHeaderToObject(ObjectHeader),
            ObjectHeader->Flags
            );
    }
    
    PhFree(ObjectHeader);
}

/**
 * Gets the current auto-dereference pool for the 
 * current thread.
 */
FORCEINLINE PPH_AUTO_POOL PhpGetCurrentAutoPool()
{
    return (PPH_AUTO_POOL)TlsGetValue(PhpAutoPoolTlsIndex);
}

/**
 * Sets the current auto-dereference pool for the 
 * current thread.
 */
FORCEINLINE VOID PhpSetCurrentAutoPool(
    __in PPH_AUTO_POOL AutoPool
    )
{
    if (!TlsSetValue(PhpAutoPoolTlsIndex, AutoPool))
        PhRaiseStatus(STATUS_UNSUCCESSFUL);
}

/**
 * Creates an auto-dereference pool and sets it 
 * as the current pool for the current thread.
 *
 * \return A pointer to an auto-dereference pool.
 * You must free the pool using PhFreeAutoPool() 
 * when you no longer need it. Always store the 
 * pointer in a local variable, and do not share 
 * the pointer with any other functions.
 */
PPH_AUTO_POOL PhCreateAutoPool()
{
    PPH_AUTO_POOL autoPool;

    autoPool = (PPH_AUTO_POOL)PhAllocate(sizeof(PH_AUTO_POOL));

    autoPool->StaticCount = 0;
    autoPool->DynamicCount = 0;
    autoPool->DynamicAllocated = 0;
    autoPool->DynamicObjects = NULL;

    // Add the pool to the stack.
    autoPool->NextPool = PhpGetCurrentAutoPool();
    PhpSetCurrentAutoPool(autoPool);

    return autoPool;
}

/**
 * Frees an auto-dereference pool.
 * The function will dereference any objects 
 * currently in the pool.
 *
 * \param AutoPool The auto-dereference pool to free.
 */
VOID PhFreeAutoPool(
    __inout PPH_AUTO_POOL AutoPool
    )
{
    PhDrainAutoPool(AutoPool);

    // Remove the pool from the stack.
    PhpSetCurrentAutoPool(AutoPool->NextPool);

    // Free the dynamic array if it hasn't been freed yet.
    if (AutoPool->DynamicObjects)
        PhFree(AutoPool->DynamicObjects);

    // Free the pool.
    PhFree(AutoPool);
}

/**
 * Adds an object to the current auto-dereference 
 * pool for the current thread.
 * If the current thread does not have an auto-dereference 
 * pool, the function raises an exception.
 *
 * \param Object A pointer to an object. The object 
 * will be dereferenced when the current auto-dereference 
 * pool is drained or freed.
 */
VOID PhaDereferenceObject(
    __in PVOID Object
    )
{
    PPH_AUTO_POOL autoPool = PhpGetCurrentAutoPool();

    // If we don't have an auto-dereference pool, 
    // we don't want to leak the object (unlike what 
    // Apple does with NSAutoreleasePool).
    if (!autoPool)
        PhRaiseStatus(STATUS_UNSUCCESSFUL);

    // See if we can use the static array.
    if (autoPool->StaticCount < PH_AUTO_POOL_STATIC_SIZE)
    {
        autoPool->StaticObjects[autoPool->StaticCount++] = Object;
        return;
    }

    // Use the dynamic array.

    // Allocate the array if we haven't already.
    if (!autoPool->DynamicObjects)
    {
        autoPool->DynamicAllocated = 64;
        autoPool->DynamicObjects = PhAllocate(
            sizeof(PVOID) * autoPool->DynamicAllocated
            );
    }

    // See if we need to resize the array.
    if (autoPool->DynamicCount == autoPool->DynamicAllocated)
    {
        autoPool->DynamicAllocated *= 2;
        autoPool->DynamicObjects = PhReAlloc(
            autoPool->DynamicObjects,
            sizeof(PVOID) * autoPool->DynamicAllocated
            );
    }

    autoPool->DynamicObjects[autoPool->DynamicCount++] = Object;
}

/**
 * Dereferences and removes all objects in an
 * auto-release pool.
 *
 * \param AutoPool The auto-release pool to drain.
 */
VOID PhDrainAutoPool(
    __in PPH_AUTO_POOL AutoPool
    )
{
    ULONG i;

    for (i = 0; i < AutoPool->StaticCount; i++)
        PhDereferenceObject(AutoPool->StaticObjects[i]);

    AutoPool->StaticCount = 0;

    if (AutoPool->DynamicObjects)
    {
        for (i = 0; i < AutoPool->DynamicCount; i++)
        {
            PhDereferenceObject(AutoPool->DynamicObjects[i]);
        }

        AutoPool->DynamicCount = 0;

        if (AutoPool->DynamicAllocated > PH_AUTO_POOL_DYNAMIC_BIG_SIZE)
        {
            AutoPool->DynamicAllocated = 0;
            PhFree(AutoPool->DynamicObjects);
            AutoPool->DynamicObjects = NULL;
        }
    }
}