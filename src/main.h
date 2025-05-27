/**
* @copyright 2020 - Max Bebök
* @license GNU-GPLv3 - see the "LICENSE" file in the root directory
*/
#pragma once

// All supported WASM types
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned long long u64;
typedef signed long long s64;
typedef float f32;
typedef double f64;

// Extra types (gets compiled to one of the above)
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef bool bool32;

typedef u32 uint32_t;
typedef s32 int32_t;
typedef u16 uint16_t;
typedef s16 int16_t;
typedef u8 uint8_t;
typedef s8 int8_t;

#define WASM_EXPORT(name) __attribute__((visibility("default"), export_name(#name))) name
#define WASM_IMPORT(name) __attribute__((visibility("default"), import_name(#name))) name

#define WASM_IMPORT_NAMED(name) __attribute__((visibility("default"), import_name(#name)))
#define WASM_EXPORT_NAMED(name) __attribute__((visibility("default"), export_name(#name)))

