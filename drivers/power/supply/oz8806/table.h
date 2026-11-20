/*****************************************************************************
* Copyright(c) O2Micro, 2014. All rights reserved.
*
* O2Micro battery gauge driver
* File: table.h
*
* This program is free software and can be edistributed and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* Jelly Star battery (Unihertz, 2040 mAh, battery IDs A55_x): the table version, battery IDs,
* OCV table and RC table are the data of the stock Jelly Star fauge module (V01.00.04), taken from
* its named data objects (table_version, battery_id, ocv_data, XAxisElement, YAxisElement,
* ZAxisElement, RCtable). No public source has this battery's table.
*****************************************************************************/
#ifndef _TABLE_H_
#define _TABLE_H_

#define OCV_DATA_NUM 	 65
#define XAxis 		 81
#define YAxis 		 6
#define ZAxis 		 6
#define BATTERY_ID_NUM 		 38

/****************************************************************************
* Struct section
*  add struct #define here if any
***************************************************************************/
typedef struct tag_one_latitude_data {
 	 int32_t 			 x;//
 	 int32_t 			 y;//
} one_latitude_data_t;

/****************************************************************************
* extern variable declaration section
***************************************************************************/
extern const char *battery_id[] ;
extern const char *table_version;

#endif
