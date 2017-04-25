#ifdef _EXCEPTION_H_
#define _EXCEPTION_H_

/* Tables */
struct bhalf {
    void *param;
    void (*handler)(void *param);
    uint8 flag;
};

/* MACROs */
#define MAX_BHALF       16              /* 16 bottom halves are allowed */
#define VECTOR_BASE     0x00000000
#define BH_MASK         (uint32)0x00000001

#define BH_HANDLING     1
#define BH_UNSUBSCRIBE  BH_HANDLING << 1

    /* The following vectors are allowed only for the kernel usage */
#define RESET           0
#define BUS_ERROR       2
#define ADDRESS_ERROR   3
#define ILLEGAL_INSTR   4
#define ZERO_DIVIDE     5
#define CHK             6
#define TRAPV           7
#define PRIVILEGE_VIOL  8
#define TRACE           9
#define UNINIT_VECTOR   15
#define SPURIOUS        24

    /* The following vectors are available for users*/
#define AUTOVECT(level) (24 + level)    /* 'level' ranges from 1-7 */
#define TRAP(no)        (32 + no)       /* 'no' ranges from 0-15 */
#define USER_VECTOR(no) (64 + no)       /* 'no' ranges from 0-191 */

    /* Bottom Halves */
#define BH_TIMEOUT      0

#endif /* _EXCEPTION_H_ */
