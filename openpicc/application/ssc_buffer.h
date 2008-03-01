#ifndef SSC_BUFFER_H_
#define SSC_BUFFER_H_

#define SSC_RX_BUFFER_SIZE_AS_UINT8 2048
#define SSC_DMA_BUFFER_COUNT 4

#if SSC_RX_BUFFER_SIZE_AS_UINT8 < DIV_ROUND_UP((ISO14443A_MAX_RX_FRAME_SIZE_IN_BITS*ISO14443A_SAMPLE_LEN),8)
#undef SSC_RX_BUFFER_SIZE_AS_UINT8
#define SSC_RX_BUFFER_SIZE_AS_UINT8 DIV_ROUND_UP((ISO14443A_MAX_RX_FRAME_SIZE_IN_BITS*ISO14443A_SAMPLE_LEN),8)
#endif

typedef enum {
	FREE=0,    /* Buffer is free */
	PENDING,   /* Buffer has been given to the DMA controller and is currently being filled */
	FULL,      /* DMA controller signalled that the buffer is full */
	PROCESSING,/* The buffer is currently processed by the consumer (e.g. decoder) */
	PREFILLED, /* The buffer has been prefilled for later usage (only used for TX) */
} ssc_dma_buffer_state_t;

enum ssc_mode {
	SSC_MODE_NONE,
	SSC_MODE_14443A,
};

typedef struct {
	enum ssc_mode mode;
	u_int16_t transfersize_ssc;
	u_int16_t transfersize_pdc;
	u_int16_t transfers;
} ssc_mode_def;

typedef struct {
	volatile ssc_dma_buffer_state_t state;
	u_int32_t len_transfers;            /* Length of the content, in transfers */
	struct {
		int overflow:1;
	} flags;
	const ssc_mode_def *reception_mode; /* Pointer to the SSC mode definition that the buffer has been loaded for (affects element size and count) */
	u_int8_t data[SSC_RX_BUFFER_SIZE_AS_UINT8];
} ssc_dma_rx_buffer_t;


typedef struct {
} ssc_dma_tx_buffer_t;

#endif /*SSC_BUFFER_H_*/