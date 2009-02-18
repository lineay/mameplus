#pragma once

#ifndef __C6280_H__
#define __C6280_H__

/* Function prototypes */
WRITE8_DEVICE_HANDLER( c6280_w );
READ8_DEVICE_HANDLER( c6280_r );

DEVICE_GET_INFO( c6280 );
#define SOUND_C6280 DEVICE_GET_INFO_NAME( c6280 )

#endif /* __C6280_H__ */
