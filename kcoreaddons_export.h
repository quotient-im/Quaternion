
#ifndef KCOREADDONS_EXPORT_H
#define KCOREADDONS_EXPORT_H

#ifdef KCOREADDONS_STATIC_DEFINE
#  define KCOREADDONS_EXPORT
#  define KCOREADDONS_NO_EXPORT
#else
#  ifndef KCOREADDONS_EXPORT
#    ifdef KF5CoreAddons_EXPORTS
        /* We are building this library */
#      define KCOREADDONS_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define KCOREADDONS_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef KCOREADDONS_NO_EXPORT
#    define KCOREADDONS_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef KCOREADDONS_DEPRECATED
#  define KCOREADDONS_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef KCOREADDONS_DEPRECATED_EXPORT
#  define KCOREADDONS_DEPRECATED_EXPORT KCOREADDONS_EXPORT KCOREADDONS_DEPRECATED
#endif

#ifndef KCOREADDONS_DEPRECATED_NO_EXPORT
#  define KCOREADDONS_DEPRECATED_NO_EXPORT KCOREADDONS_NO_EXPORT KCOREADDONS_DEPRECATED
#endif

#define DEFINE_NO_DEPRECATED 0
#if DEFINE_NO_DEPRECATED
# define KCOREADDONS_NO_DEPRECATED
#endif

#endif
