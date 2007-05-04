/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */


#define  DOSNET_DEVICE  "dsn0"

#define  DOSNET_FAKED_ETH_ADDRESS   "dbx\x90xx"
#define  DOSNET_BROADCAST_ADDRESS   "db\xff\x90xx"
#define  DOSNET_DEVICE_ETH_ADDRESS  "db\0\0db"

#define DOSNET_BROADCAST_TYPE   0x90ff
#define DOSNET_INVALID_TYPE     0x90fe

/* Each dosemu will use from 0x9000 onwards. */
#define DOSNET_TYPE_BASE        0x9000

/* It is very important to differentiate between a generic dosnet address
   and address assigned to dsn0. If you change anything here, be sure
   to change the test in device specific type extraction code in dosnet.c.
*/

#define  TAP_DEVICE  "tap%d"
