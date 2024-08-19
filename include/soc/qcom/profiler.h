/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017, 2020-2021 The Linux Foundation. All rights reserved.
Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
*/

#ifndef __PROFILER_H_
#define __PROFILER_H_

/* Configure the number of channels per chipset. */
#define LLCC_CHANNELS 6
#define CABO_CHANNELS 6
#define NUM_REG_TYPES 2
#define HF_COUNTERS 16
#define SF_COUNTERS 12
#define GEMNOC_COUNTERS 8

/* Structure for backward compatibility. */
struct profiler_bw_cntrs_req_m {
	uint32_t llcc_total;
	uint32_t llcc_rd;
	uint32_t llcc_wr;
	uint32_t cabo_total;
	uint32_t cabo_rd;
	uint32_t cabo_wr;
	uint32_t cmd;
};

struct profiler_bw_cntrs_req {
	uint32_t cmd;
	uint32_t bwEnableFlags;
	int llcc_values[LLCC_CHANNELS * NUM_REG_TYPES];
	int cabo_values[CABO_CHANNELS * NUM_REG_TYPES];
	int gemnoc_values[LLCC_CHANNELS * GEMNOC_COUNTERS];
	int mmnoc_hf_values[HF_COUNTERS];
	int mmnoc_sf_values[SF_COUNTERS];
};

struct reg_offset {
	int llcc_offset[LLCC_CHANNELS * NUM_REG_TYPES];
	int cabo_offset[CABO_CHANNELS * NUM_REG_TYPES];
	int gemnoc_offset[LLCC_CHANNELS * GEMNOC_COUNTERS];
	int mmnoc_hf_offset[HF_COUNTERS];
	int mmnoc_sf_offset[SF_COUNTERS];
};

struct device_param_init {
	int num_llcc_channels;
	uint32_t llcc_base;
	uint32_t llcc_map_size;
	int num_cabo_channels;
	int num_gemnoc_metrics;
	uint32_t gemnoc_base;
	uint32_t gemnoc_map_size;
	int num_hf_metrics;
	int num_sf_metrics;
	uint32_t mmnoc_base;
	uint32_t mmnoc_map_size;
};


/* Error types */
enum tz_bw_svc_err {
	E_BW_SUCCESS = 0,       /* Operation successful    */
	E_BW_FAILURE = 1,       /* Operation failed due to unknown err   */
	E_BW_NULL_PARAM = 2,    /* Null Parameter          */
	E_BW_INVALID_ARG = 3,   /* Arg is not recognized   */
	E_BW_BAD_ADDRESS = 4,   /* Ptr arg is bad address  */
	E_BW_INVALID_ARG_LEN = 5,       /* Arg length is wrong   */
	E_BW_NOT_SUPPORTED = 6, /* Operation not supported */
	E_BW_NOT_PERMITTED = 7, /* Operation not permitted on platform   */
	E_BW_TIME_LOCKED = 8,   /* Operation not permitted right now     */
	E_BW_VERSION_BC      = (1 << 8), /* Backward compatible check for version  */
	E_BW_RESERVED = 0x7FFFFFFF
};

#define TZ_BW_SVC_VERSION (1)
#define PROFILER_IOC_MAGIC    0x98

#define PROFILER_IOCTL_GET_BW_INFO \
	_IOWR(PROFILER_IOC_MAGIC, 1, struct profiler_bw_cntrs_req)

#define PROFILER_IOCTL_SET_OFFSETS \
	_IOWR(PROFILER_IOC_MAGIC, 2, struct reg_offset)

#define PROFILER_IOCTL_DEVICE_INIT \
	_IOWR(PROFILER_IOC_MAGIC, 3, struct device_param_init)

#define PROFILER_IOCTL_GET_BW_INFO_BC \
	_IOWR(PROFILER_IOC_MAGIC, 4, struct profiler_bw_cntrs_req_m)

/* Command types */
enum tz_bw_svc_cmd {
	TZ_BW_SVC_START_ID = 0x00000001,
	TZ_BW_SVC_GET_ID = 0x00000002,
	TZ_BW_SVC_STOP_ID = 0x00000003,
	TZ_BW_SVC_LAST_ID = 0x7FFFFFFF
};
/* Start Request */
struct tz_bw_svc_start_req {
	enum tz_bw_svc_cmd cmd_id;
	uint32_t version;
	uint32_t bwEnableFlags;
} __packed;

/* Get Request */
struct tz_bw_svc_get_req {
	enum tz_bw_svc_cmd cmd_id;
	uint64_t buf_ptr;
	uint32_t buf_size;
	uint32_t type; // Stop : 0, Reset : 1
} __packed;

/*  Stop Request */
struct tz_bw_svc_stop_req {
	enum tz_bw_svc_cmd cmd_id;
} __packed;

struct tz_bw_svc_resp {
	enum tz_bw_svc_cmd cmd_id;
	enum tz_bw_svc_err status;
} __packed;

union tz_bw_svc_req {
	struct tz_bw_svc_start_req start_req;
	struct tz_bw_svc_get_req get_req;
	struct tz_bw_svc_stop_req stop_req;
} __packed;

struct tz_bw_svc_buf {
	union tz_bw_svc_req bwreq;
	struct tz_bw_svc_resp bwresp;
	uint32_t req_size;
} __packed;


#endif /* __PROFILER_H_ */
