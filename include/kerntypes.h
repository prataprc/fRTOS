#ifdef _KERNTYPES_H_
#define _KERNTYPES_H_

typedef unsigned char   u8;
typedef signed char     s8;
typedef unsigned short  u16;
typedef signed short    s16;
typedef unsigned int    u32;
typedef signed int      s32;
typedef unsigned char   uint8;
typedef signed char     sint8;
typedef unsigned short  uint16;
typedef signed short    sint16;
typedef unsigned int    uint32;
typedef signed int      sint32;

typedef u32     msec_t;
typedef u32     sec_t;
typedef u32     timid_t;

typedef u32     cpuirq_t;   /* FIXME: This specific to processor */

#endif /* _KERNTYPES_H_ */
