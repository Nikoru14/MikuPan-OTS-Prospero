/*
 * MikuPan PS5 title: thread_local destructor registration.
 *
 * libc++abi's __cxa_thread_atexit forwards to the C library's
 * __cxa_thread_atexit_impl, which no PS5 system module exports, so a title
 * that uses thread_local objects with destructors cannot be converted. This
 * provides it inside the executable: each thread keeps a list of destructors
 * in a pthread key, and the key's destructor runs them, newest first, when
 * the thread exits. The main thread's list is left to process teardown.
 */

#include <pthread.h>
#include <stdlib.h>

namespace
{
struct ThreadDestructor
{
    void (*destroy)(void *);
    void *object;
    ThreadDestructor *next;
};

pthread_key_t thread_destructor_key;
pthread_once_t thread_destructor_once = PTHREAD_ONCE_INIT;

void RunThreadDestructors(void *head)
{
    auto *entry = static_cast<ThreadDestructor *>(head);
    while (entry != nullptr)
    {
        ThreadDestructor *next = entry->next;
        entry->destroy(entry->object);
        free(entry);
        entry = next;
    }
}

void CreateThreadDestructorKey()
{
    pthread_key_create(&thread_destructor_key, RunThreadDestructors);
}
} // namespace

extern "C" int __cxa_thread_atexit_impl(void (*destroy)(void *), void *object,
                                        void *dso_handle)
{
    (void)dso_handle;

    pthread_once(&thread_destructor_once, CreateThreadDestructorKey);

    auto *entry = static_cast<ThreadDestructor *>(malloc(sizeof(ThreadDestructor)));
    if (entry == nullptr)
    {
        return -1;
    }

    entry->destroy = destroy;
    entry->object = object;
    entry->next = static_cast<ThreadDestructor *>(
        pthread_getspecific(thread_destructor_key));
    return pthread_setspecific(thread_destructor_key, entry);
}
