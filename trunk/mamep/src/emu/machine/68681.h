#ifndef _68681_H
#define _68681_H

typedef struct _duart68681_config duart68681_config;
struct _duart68681_config
{
	int frequency;
	void (*irq_handler)(const device_config *device, UINT8 vector);
	void (*tx_callback)(const device_config *device, int channel, UINT8 data);
	UINT8 (*input_port_read)(const device_config *device);
	void (*output_port_write)(const device_config *device, UINT8 data);
};

#define DUART68681 DEVICE_GET_INFO_NAME(duart68681)
DEVICE_GET_INFO(duart68681);

READ8_DEVICE_HANDLER(duart68681_r);
WRITE8_DEVICE_HANDLER(duart68681_w);

void duart68681_rx_data( const device_config* device, int ch, UINT8 data );

#endif //_68681_H