#ifndef __MTK_H__
#define __MTK_H__

#define MAC_ADDR_LENGTH		6
#define MAX_NUMBER_OF_MAC	256
typedef unsigned char 	UCHAR;
typedef char		CHAR;
typedef unsigned int	UINT32;
typedef unsigned short	USHORT;
typedef short		SHORT;
typedef unsigned long	ULONG;

#if WIRELESS_EXT <= 11
#ifndef SIOCDEVPRIVATE
#define SIOCDEVPRIVATE				0x8BE0
#endif
#define SIOCIWFIRSTPRIV				SIOCDEVPRIVATE
#endif

#define RTPRIV_IOCTL_SET			(SIOCIWFIRSTPRIV + 0x02)
#define RTPRIV_IOCTL_SHOW           (SIOCIWFIRSTPRIV + 0x11)
#define RTPRIV_IOCTL_GSITESURVEY	(SIOCIWFIRSTPRIV + 0x0D)

#endif // __MTK_H__

