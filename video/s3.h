/*
 * video/s3.h - Prototypes for S3-card specifics functions
 */

#ifndef S3_H
#define S3_H

#define S3_801	1
#define S3_805	2
#define S3_911	3
#define S3_928	4

#define S3_NORMAL_DAC	1
#define S3_ATT20C505_DAC 2
#define S3_BT485_DAC	3
#define S3_TI3020_DAC	4

#define S3_DAC_INDEX_REG	0x3c6
#define S3_DAC_DATA_REG		0x3c7

#define S3_TI3020_ID		0x20
#define S3_TI3020_ID2		0x3F

#define BT_WRITE_ADDR           0x00
#define BT_RAMDAC_DATA          0x01
#define BT_PIXEL_MASK           0x02
#define BT_READ_ADDR            0x03
#define BT_CURS_WR_ADDR         0x04
#define BT_CURS_DATA            0x05
#define BT_COMMAND_REG_0        0x06
#define BT_CURS_RD_ADDR         0x07
#define BT_COMMAND_REG_1        0x08
#define BT_COMMAND_REG_2        0x09
#define BT_STATUS_REG           0x0A
#define BT_CURS_RAM_DATA        0x0B
#define BT_CURS_X_LOW           0x0C
#define BT_CURS_X_HIGH          0x0D
#define BT_CURS_Y_LOW           0x0E
#define BT_CURS_Y_HIGH          0x0F


extern void vga_init_s3(void);

#endif
/* End of video/s3.h */
